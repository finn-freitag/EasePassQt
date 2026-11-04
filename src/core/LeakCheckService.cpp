#include "LeakCheckService.h"

#include "AppSettings.h"

#include <QCryptographicHash>
#include <QMetaObject>
#include <QNetworkRequest>
#include <openssl/crypto.h>

namespace EasePass::Core {

LeakCheckService::LeakCheckService(std::shared_ptr<INetworkClient> client, AppSettings *settings, const IClock *clock,
                                   QObject *parent)
    : QObject(parent), m_client(std::move(client)), m_settings(settings),
      m_clock(clock ? clock : &SystemClock::instance()) {}

bool LeakCheckService::isEnabled() const {
    return m_settings ? m_settings->verifyNotLeaked() : AppSettings::Defaults::VerifyNotLeaked;
}

QByteArray LeakCheckService::sha1Hex(const SecretString &password) {
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArrayView(password.constData(), qsizetype(password.size())));
    return hash.result().toHex().toUpper();
}

int LeakCheckService::countForSuffix(const QByteArray &rangeBody, const QByteArray &suffix, bool *ok) {
    bool valid = true;
    int found = 0;
    for (QByteArray line : rangeBody.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        const qsizetype colon = line.indexOf(':');
        if (colon != 35) { // 35 hex chars suffix
            valid = false;
            continue;
        }
        if (line.first(colon).compare(suffix, Qt::CaseInsensitive) == 0) {
            bool numOk = false;
            const int n = line.mid(colon + 1).toInt(&numOk);
            if (numOk)
                found = n;
            else
                valid = false;
        }
    }
    if (ok)
        *ok = valid;
    return found;
}

LeakCheckService::Result LeakCheckService::evaluate(const QByteArray &body, const QByteArray &suffix) const {
    Result r;
    bool ok = false;
    const int count = countForSuffix(body, suffix, &ok);
    if (count > 0) {
        r.status = Status::Leaked;
        r.count = count;
    } else if (ok) {
        r.status = Status::NotLeaked;
    } else {
        r.errorString = tr("Invalid response from the leak check service.");
    }
    return r;
}

void LeakCheckService::deliver(Callback callback, const Result &result) {
    QMetaObject::invokeMethod(
        this,
        [this, callback = std::move(callback), result]() {
            emit checkFinished(result.status, result.count);
            if (callback)
                callback(result);
        },
        Qt::QueuedConnection);
}

void LeakCheckService::check(const SecretString &password, Callback callback) {
    if (!isEnabled()) {
        deliver(std::move(callback), {Status::Unknown, 0, tr("Leak check is disabled.")});
        return;
    }
    if (password.isEmpty() || !m_client) {
        deliver(std::move(callback), {Status::Unknown, 0, tr("Nothing to check.")});
        return;
    }

    QByteArray hex = sha1Hex(password);
    const QByteArray prefix = hex.left(5);
    const QByteArray suffix = hex.mid(5);
    OPENSSL_cleanse(hex.data(), size_t(hex.size()));

    auto it = m_cache.constFind(prefix);
    if (it != m_cache.constEnd() && m_clock->currentSecsSinceEpoch() - it->fetchedAt < m_cacheTtl) {
        deliver(std::move(callback), evaluate(it->body, suffix));
        return;
    }

    auto &waiters = m_pending[prefix];
    waiters.append({suffix, std::move(callback)});
    if (waiters.size() > 1)
        return; // request for this prefix already in flight

    QNetworkRequest request(QUrl(m_baseUrl + QString::fromLatin1(prefix)));
    request.setRawHeader("Add-Padding", "true");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EasePass"));
    QPointer<LeakCheckService> self(this);
    m_client->get(request, [self, prefix](const NetworkResponse &resp) {
        if (self)
            self->handleResponse(prefix, resp);
    });
}

void LeakCheckService::handleResponse(const QByteArray &prefix, const NetworkResponse &resp) {
    const QList<Waiter> waiters = m_pending.take(prefix);
    if (resp.isSuccess()) {
        m_cache.insert(prefix, {resp.data, m_clock->currentSecsSinceEpoch()});
        for (const Waiter &w : waiters)
            deliver(w.callback, evaluate(resp.data, w.suffix));
        return;
    }
    Result r;
    r.errorString = resp.errorString.isEmpty() ? tr("Leak check failed (HTTP %1).").arg(resp.statusCode)
                                               : resp.errorString;
    for (const Waiter &w : waiters)
        deliver(w.callback, r);
}

} // namespace EasePass::Core
