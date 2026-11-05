#include "PasswordGeneratorViewModel.h"

#include "AppSettings.h"
#include "DatabaseService.h"
#include "LeakCheckService.h"
#include "PasswordGenerator.h"
#include "PasswordStrength.h"
#include "SecretString.h"

namespace EasePass::App {

using namespace EasePass::Core;

PasswordGeneratorViewModel::PasswordGeneratorViewModel(AppSettings *settings, DatabaseService *service,
                                                       LeakCheckService *leak, QObject *parent)
    : QObject(parent), m_settings(settings), m_service(service), m_safetyChecks(8, QVariant()) {
    // T23: the last generated password must not outlive the session.
    if (m_service)
        connect(m_service, &DatabaseService::closed, this, [this] { setPassword(QString()); });
    if (leak) {
        QPointer<LeakCheckService> l(leak);
        m_checker = [l](const QString &password, std::function<void(LeakResult)> done) {
            if (!l) {
                done(LeakResult::Unknown);
                return;
            }
            l->check(SecretString(password), [done](const LeakCheckService::Result &r) {
                done(r.status == LeakCheckService::Status::Leaked      ? LeakResult::Leaked
                     : r.status == LeakCheckService::Status::NotLeaked ? LeakResult::NotLeaked
                                                                       : LeakResult::Unknown);
            });
        };
    }
}

void PasswordGeneratorViewModel::setLeakChecker(LeakChecker checker) { m_checker = std::move(checker); }

bool PasswordGeneratorViewModel::leakCheckEnabled() const {
    // C#: AppSettings.DisableLeakedPasswords
    const bool verify = m_settings ? m_settings->verifyNotLeaked() : AppSettings::Defaults::VerifyNotLeaked;
    return verify && m_checker;
}

void PasswordGeneratorViewModel::setPassword(const QString &password) {
    if (password == m_password)
        return;
    ++m_generation; // a typed password ends a running generate()
    if (m_busy) {
        m_busy = false;
        emit busyChanged();
    }
    m_password = password;
    emit passwordChanged();
    updateSafety(QVariant());
    if (password.isEmpty() || !leakCheckEnabled())
        return;
    const quint64 generation = m_generation;
    QPointer<PasswordGeneratorViewModel> self(this);
    m_checker(password, [self, generation](LeakResult r) {
        if (self && generation == self->m_generation && r != LeakResult::Unknown)
            self->updateSafety(r == LeakResult::NotLeaked);
    });
}

void PasswordGeneratorViewModel::generate() {
    if (m_busy)
        return;
    ++m_generation;
    m_attempts = 0;
    m_busy = true;
    emit busyChanged();
    attempt();
}

void PasswordGeneratorViewModel::attempt() {
    const int length = m_settings ? m_settings->passwordLength() : AppSettings::Defaults::PasswordLength;
    const QString chars = m_settings ? m_settings->passwordChars() : AppSettings::Defaults::PasswordChars;
    const QString candidate = PasswordGenerator::generateFromCharset(length, chars);
    ++m_attempts;
    if (!leakCheckEnabled()) {
        finish(candidate, LeakResult::Unknown);
        return;
    }
    const quint64 generation = m_generation;
    QPointer<PasswordGeneratorViewModel> self(this);
    m_checker(candidate, [self, generation, candidate](LeakResult r) {
        if (!self || generation != self->m_generation)
            return; // cancelled (password typed) or destroyed
        if (r == LeakResult::Leaked && self->m_attempts < MaxLeakRetries)
            self->attempt();
        else
            self->finish(candidate, r);
    });
}

void PasswordGeneratorViewModel::finish(const QString &password, LeakResult leak) {
    m_busy = false;
    m_password = password;
    emit passwordChanged();
    updateSafety(leak == LeakResult::Unknown ? QVariant() : QVariant(leak == LeakResult::NotLeaked));
    emit busyChanged();
    emit generated(password);
}

void PasswordGeneratorViewModel::updateSafety(const QVariant &notLeaked) {
    if (m_password.isEmpty()) {
        m_safetyChecks = QVariantList(8, QVariant());
    } else {
        const int minLength = m_settings ? m_settings->passwordLength() : PasswordStrength::DefaultMinLength;
        const auto e = m_service && m_service->isOpen()
                           ? PasswordStrength::evaluate(m_password, *m_service, false, minLength)
                           : PasswordStrength::evaluate(m_password, minLength);
        // Order of PasswordSafetyChart (see ItemViewModel::updateSafety).
        m_safetyChecks = {e.hasLower, e.hasUpper,  e.meetsMinLength,    notLeaked,
                          e.hasSpecial, e.hasNumber, e.noCommonSequences, e.notReused};
    }
    emit safetyChanged();
}

} // namespace EasePass::App
