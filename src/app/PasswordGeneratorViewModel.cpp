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
    if (m_settings)
        connect(m_settings, &AppSettings::passwordGeneratorPluginChanged, this,
                &PasswordGeneratorViewModel::generatorIdChanged);
}

void PasswordGeneratorViewModel::setExternalGenerators(QList<ExternalGenerator> generators) {
    const QString before = generatorId();
    m_external = std::move(generators);
    emit generatorsChanged();
    if (generatorId() != before)
        emit generatorIdChanged();
}

QVariantList PasswordGeneratorViewModel::generators() const {
    QVariantList result{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), tr("Built-in generator")}}};
    for (const ExternalGenerator &generator : m_external)
        result << QVariantMap{{QStringLiteral("id"), generator.id}, {QStringLiteral("name"), generator.name}};
    return result;
}

const PasswordGeneratorViewModel::ExternalGenerator *PasswordGeneratorViewModel::external() const {
    const QString id = m_settings ? m_settings->passwordGeneratorPlugin() : m_generatorId;
    if (id.isEmpty())
        return nullptr;
    for (const ExternalGenerator &generator : m_external) {
        if (generator.id == id && generator.generate)
            return &generator;
    }
    return nullptr;
}

QString PasswordGeneratorViewModel::generatorId() const {
    const ExternalGenerator *generator = external();
    return generator ? generator->id : QString();
}

void PasswordGeneratorViewModel::setGeneratorId(const QString &id) {
    if (m_settings) {
        m_settings->setPasswordGeneratorPlugin(id); // emits generatorIdChanged through the setting
    } else if (m_generatorId != id) {
        m_generatorId = id;
        emit generatorIdChanged();
    }
}

void PasswordGeneratorViewModel::setErrorMessage(const QString &message) {
    if (m_errorMessage == message)
        return;
    m_errorMessage = message;
    emit errorMessageChanged();
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
    setErrorMessage(QString());
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
    setErrorMessage(QString());
    m_busy = true;
    emit busyChanged();
    attempt();
}

void PasswordGeneratorViewModel::attempt() {
    const int length = m_settings ? m_settings->passwordLength() : AppSettings::Defaults::PasswordLength;
    const QString chars = m_settings ? m_settings->passwordChars() : AppSettings::Defaults::PasswordChars;
    const ExternalGenerator *generator = external();
    if (!generator) {
        check(PasswordGenerator::generateFromCharset(length, chars));
        return;
    }
    const quint64 generation = m_generation;
    QPointer<PasswordGeneratorViewModel> self(this);
    const QString name = generator->name;
    const auto generate = generator->generate; // the list may change while the plugin works
    generate(length, chars, leakCheckEnabled(), [self, generation, name](const QString &password, const QString &error) {
        if (!self || generation != self->m_generation)
            return; // cancelled (password typed) or destroyed
        if (!error.isEmpty()) {
            self->fail(tr("%1 could not generate a password: %2").arg(name, error));
            return;
        }
        // The text becomes a real password: nothing empty, huge or with control characters.
        bool usable = !password.isEmpty() && password.size() <= MaxExternalPasswordLength;
        for (const QChar c : password)
            usable = usable && c.category() != QChar::Other_Control && c.category() != QChar::Other_Format;
        if (!usable) {
            self->fail(tr("%1 returned a password that cannot be used.").arg(name));
            return;
        }
        self->check(password);
    });
}

void PasswordGeneratorViewModel::check(const QString &candidate) {
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

void PasswordGeneratorViewModel::fail(const QString &message) {
    m_busy = false;
    setErrorMessage(message);
    emit busyChanged();
    emit failed(message);
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
