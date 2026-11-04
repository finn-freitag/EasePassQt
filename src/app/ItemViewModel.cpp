#include "ItemViewModel.h"

#include "AppSettings.h"
#include "ClipboardService.h"
#include "DatabaseService.h"
#include "LeakCheckService.h"
#include "PasswordStrength.h"
#include "TotpHelper.h"
#include "TotpTimerService.h"

namespace EasePass::App {

using namespace EasePass::Core;

namespace {
QVariantList unknownChecks() { return QVariantList(8, QVariant()); }

bool parseIntInRange(const QString &s, int min, int max, int *out = nullptr) {
    bool ok = false;
    const int v = s.trimmed().toInt(&ok);
    if (out)
        *out = v;
    return ok && v >= min && v <= max;
}
// TotpHelper::base32Decode skips unknown characters, so check the alphabet explicitly
// (spaces, '-' and trailing '=' padding are allowed, as the decoder strips them).
bool isValidBase32(const QString &secret) {
    QString clean = secret.trimmed().remove(u' ').remove(u'-').toUpper();
    while (clean.endsWith(u'='))
        clean.chop(1);
    if (clean.isEmpty())
        return false;
    for (QChar c : std::as_const(clean)) {
        const char ch = c.toLatin1();
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '2' && ch <= '7')))
            return false;
    }
    return !TotpHelper::base32Decode(clean).isEmpty();
}
} // namespace

ItemViewModel::ItemViewModel(DatabaseService *service, TotpTimerService *totpTimer, QObject *parent)
    : QObject(parent), m_service(service), m_totpTimer(totpTimer), m_safetyChecks(unknownChecks()) {
    connect(m_totpTimer, &TotpTimerService::tick, this, &ItemViewModel::updateTotp);
    connect(m_service, &DatabaseService::closed, this, &ItemViewModel::unload);
}

ItemViewModel::~ItemViewModel() {
    if (m_totpSubscribed)
        m_totpTimer->release();
}

void ItemViewModel::setLeakCheckService(LeakCheckService *leak) { m_leak = leak; }
void ItemViewModel::setClipboardService(ClipboardService *clipboard) { m_clipboard = clipboard; }
void ItemViewModel::setSettings(AppSettings *settings) { m_settings = settings; }

int ItemViewModel::minPasswordLength() const {
    return m_settings ? m_settings->passwordLength() : PasswordStrength::DefaultMinLength;
}

// --- Loading ------------------------------------------------------------------------

bool ItemViewModel::load(int index) {
    if (!m_service->isOpen() || index < 0 || index >= m_service->itemCount())
        return false;
    setLoaded(index, m_service->items().at(index), true);
    return true;
}

void ItemViewModel::newItem() { setLoaded(-1, PasswordItem(), true); }

void ItemViewModel::unload() { setLoaded(-1, PasswordItem(), false); }

void ItemViewModel::setLoaded(int index, const PasswordItem &item, bool hasItem) {
    m_index = index;
    m_hasItem = hasItem;
    m_original = item;
    m_buffer = item;
    emit loadedChanged();
    afterBufferChange(true, true);
}

void ItemViewModel::cancel() {
    m_buffer = m_original;
    afterBufferChange(true, true);
}

// --- Buffer -------------------------------------------------------------------------

template <typename T> void ItemViewModel::setField(T PasswordItem::*field, const T &value) {
    if (m_buffer.*field == value)
        return;
    m_buffer.*field = value;
    afterBufferChange(false, false);
}

void ItemViewModel::setDisplayName(const QString &v) { setField(&PasswordItem::displayName, v); }
void ItemViewModel::setUsername(const QString &v) { setField(&PasswordItem::username, v); }
void ItemViewModel::setEmail(const QString &v) { setField(&PasswordItem::email, v); }
void ItemViewModel::setWebsite(const QString &v) { setField(&PasswordItem::website, v); }
void ItemViewModel::setNotes(const QString &v) { setField(&PasswordItem::notes, v); }
void ItemViewModel::setTags(const QStringList &v) { setField(&PasswordItem::tags, v); }

void ItemViewModel::setPassword(const QString &v) {
    if (m_buffer.password == v)
        return;
    m_buffer.password = v;
    afterBufferChange(true, false);
}

void ItemViewModel::setSecret(const QString &v) {
    if (m_buffer.secret == v)
        return;
    m_buffer.secret = v;
    afterBufferChange(false, true);
}
void ItemViewModel::setDigits(const QString &v) {
    if (m_buffer.digits == v)
        return;
    m_buffer.digits = v;
    afterBufferChange(false, true);
}
void ItemViewModel::setInterval(const QString &v) {
    if (m_buffer.interval == v)
        return;
    m_buffer.interval = v;
    afterBufferChange(false, true);
}
void ItemViewModel::setAlgorithm(const QString &v) {
    if (m_buffer.algorithm == v)
        return;
    m_buffer.algorithm = v;
    afterBufferChange(false, true);
}

void ItemViewModel::afterBufferChange(bool passwordChanged, bool totpChanged) {
    emit bufferChanged();
    if (passwordChanged)
        updateSafety();
    if (totpChanged) {
        updateTotpSubscription();
        updateTotp(m_totpTimer->now());
    }
}

bool ItemViewModel::dirty() const { return m_hasItem && m_buffer.toJson() != m_original.toJson(); }

QStringList ItemViewModel::validationErrors() const {
    QStringList errors;
    if (!m_hasItem)
        return errors;
    // C#: AddItemPage requires a display name.
    if (m_buffer.displayName.trimmed().isEmpty())
        errors << tr("Name must not be empty");
    if (m_buffer.has2FA()) {
        if (!isValidBase32(m_buffer.secret))
            errors << tr("2FA secret is not valid Base32");
        if (!parseIntInRange(m_buffer.digits, 4, 10))
            errors << tr("2FA digits must be a number between 4 and 10");
        if (!parseIntInRange(m_buffer.interval, 1, 3600))
            errors << tr("2FA interval must be a number of seconds between 1 and 3600");
        const QString algo = m_buffer.algorithm.trimmed().toUpper();
        if (algo != QLatin1String("SHA1") && algo != QLatin1String("SHA256") && algo != QLatin1String("SHA512"))
            errors << tr("2FA algorithm must be SHA1, SHA256 or SHA512");
    }
    return errors;
}

// --- Commit / delete ----------------------------------------------------------------

bool ItemViewModel::commit() {
    if (!m_hasItem)
        return false;
    const QStringList errors = validationErrors();
    if (!errors.isEmpty()) {
        emit commitFailed(errors);
        return false;
    }
    bool ok;
    int index = m_index;
    if (m_index < 0) {
        ok = m_service->addItem(m_buffer);
        index = m_service->itemCount() - 1;
    } else {
        // Clicks may have been registered since load() (copy actions); keep them.
        if (m_index < m_service->itemCount())
            m_buffer.clicks = m_service->items().at(m_index).clicks;
        ok = m_service->updateItem(m_index, m_buffer);
    }
    if (!ok) {
        emit commitFailed({tr("The item could not be saved (database closed or read-only)")});
        return false;
    }
    setLoaded(index, m_buffer, true);
    emit committed(index);
    return true;
}

bool ItemViewModel::deleteItem() {
    if (!m_hasItem || m_index < 0)
        return false;
    const int index = m_index;
    if (!m_service->deleteItem(index))
        return false;
    unload();
    emit deleted(index);
    return true;
}

bool ItemViewModel::applyOtpauthUri(const QString &uri) {
    TotpParameters params;
    if (!TotpHelper::parseOtpauthUri(uri, params))
        return false;
    m_buffer.secret = params.secret;
    m_buffer.digits = QString::number(params.digits);
    m_buffer.interval = QString::number(params.period);
    m_buffer.algorithm = params.algorithm;
    afterBufferChange(false, true);
    return true;
}

// --- Strength / leak ----------------------------------------------------------------

void ItemViewModel::updateSafety() {
    const quint64 generation = ++m_leakGeneration;
    if (m_buffer.password.isEmpty()) {
        m_safetyChecks = unknownChecks();
        m_leakState = LeakState::Unknown;
        emit safetyChanged();
        return;
    }
    // existingSingleTime: a saved item counts itself once (C#: EvaluatePassword(pw, true)).
    const auto e = m_service->isOpen()
                       ? PasswordStrength::evaluate(m_buffer.password, *m_service, m_index >= 0, minPasswordLength())
                       : PasswordStrength::evaluate(m_buffer.password, minPasswordLength());
    // Order of Controls/PasswordSafetyChart: lower, upper, length, leaked, special, digits,
    // predictability, seen before. Index 3 is filled by the leak check.
    m_safetyChecks = {e.hasLower, e.hasUpper, e.meetsMinLength, QVariant(), e.hasSpecial,
                      e.hasNumber, e.noCommonSequences, e.notReused};

    if (m_leak && m_leak->isEnabled()) {
        m_leakState = LeakState::Checking;
        QPointer<ItemViewModel> self(this);
        m_leak->check(SecretString(m_buffer.password), [self, generation](const LeakCheckService::Result &r) {
            if (!self || generation != self->m_leakGeneration)
                return; // password changed meanwhile
            switch (r.status) {
            case LeakCheckService::Status::NotLeaked:
                self->m_leakState = LeakState::NotLeaked;
                self->m_safetyChecks[3] = true;
                break;
            case LeakCheckService::Status::Leaked:
                self->m_leakState = LeakState::Leaked;
                self->m_safetyChecks[3] = false;
                break;
            case LeakCheckService::Status::Unknown:
                self->m_leakState = LeakState::Unknown;
                self->m_safetyChecks[3] = QVariant();
                break;
            }
            emit self->safetyChanged();
        });
        // The callback may already have run synchronously (cache hit).
        if (generation == m_leakGeneration && m_leakState != LeakState::Checking)
            return;
    } else {
        m_leakState = LeakState::Unknown;
    }
    emit safetyChanged();
}

// --- TOTP ---------------------------------------------------------------------------

void ItemViewModel::updateTotpSubscription() {
    const bool want = m_hasItem && m_buffer.has2FA();
    if (want == m_totpSubscribed)
        return;
    m_totpSubscribed = want;
    if (want)
        m_totpTimer->acquire();
    else
        m_totpTimer->release();
}

void ItemViewModel::updateTotp(qint64 now) {
    QString code;
    int remaining = 0;
    double progress = 0.0;
    int digits = 0, period = 0;
    if (m_hasItem && m_buffer.has2FA() && parseIntInRange(m_buffer.digits, 4, 10, &digits)
        && parseIntInRange(m_buffer.interval, 1, 3600, &period)) {
        code = TotpHelper::generateToken(m_buffer.secret.trimmed(), now, digits, period, m_buffer.algorithm);
        remaining = TotpHelper::getRemainingSeconds(period, now);
        progress = TotpHelper::getProgress(period, now);
    }
    if (code == m_totpCode && remaining == m_totpRemaining && qFuzzyCompare(1.0 + progress, 1.0 + m_totpProgress))
        return;
    m_totpCode = code;
    m_totpRemaining = remaining;
    m_totpProgress = progress;
    emit totpChanged();
}

// --- Clipboard ----------------------------------------------------------------------

void ItemViewModel::registerClick() {
    // C#: copying counts as a "click" for the popularity sort.
    if (m_index >= 0 && m_service->isOpen())
        m_service->registerClick(m_index, m_totpTimer->clock());
}

void ItemViewModel::copyPassword() {
    if (!m_clipboard || m_buffer.password.isEmpty())
        return;
    m_clipboard->copy(m_buffer.password, true);
    registerClick();
}
void ItemViewModel::copyUsername() {
    if (m_clipboard)
        m_clipboard->copy(m_buffer.username, false);
}
void ItemViewModel::copyEmail() {
    if (m_clipboard)
        m_clipboard->copy(m_buffer.email, false);
}
void ItemViewModel::copyTotp() {
    if (m_clipboard && !m_totpCode.isEmpty())
        m_clipboard->copy(m_totpCode, true);
}

} // namespace EasePass::App
