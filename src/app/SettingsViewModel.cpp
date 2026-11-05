#include "SettingsViewModel.h"

#include "AppSettings.h"
#include "DatabaseService.h"

#include <QVariantMap>
#include <algorithm>

namespace EasePass::App {

namespace {
struct Language {
    const char *tag;
    const char *name;
};
// C#: Strings/LanguageList.resw (native names, not translated).
constexpr Language kLanguages[] = {
    {"en-US", "English"},
    {"de-DE", "Deutsch"},
    {"pt-BR", "Português (Brasil)"},
};
} // namespace

SettingsViewModel::SettingsViewModel(Core::AppSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings), m_startLanguage(settings->language()) {
    connect(m_settings, &Core::AppSettings::passwordCharsChanged, this, &SettingsViewModel::passwordCharsValidChanged);
    connect(m_settings, &Core::AppSettings::languageChanged, this, &SettingsViewModel::languageChanged);
}

void SettingsViewModel::setDatabaseService(Core::DatabaseService *service) {
    if (m_service == service)
        return;
    if (m_service)
        disconnect(m_service, nullptr, this, nullptr);
    m_service = service;
    if (m_service) {
        connect(m_service, &Core::DatabaseService::stateChanged, this, &SettingsViewModel::canResetPopularityChanged);
        connect(m_service, &Core::DatabaseService::readOnlyChanged, this, &SettingsViewModel::canResetPopularityChanged);
    }
    emit canResetPopularityChanged();
}

// The generator needs at least two distinct characters to make sense.
bool SettingsViewModel::passwordCharsValid() const {
    const QString chars = m_settings->passwordChars();
    for (QChar c : chars)
        if (c != chars.front())
            return true;
    return false;
}

QVariantList SettingsViewModel::languages() {
    QVariantList list;
    for (const Language &l : kLanguages)
        list << QVariantMap{{QStringLiteral("tag"), QString::fromLatin1(l.tag)},
                            {QStringLiteral("name"), QString::fromUtf8(l.name)}};
    return list;
}

int SettingsViewModel::languageIndex() const {
    const QString tag = m_settings->language();
    for (int i = 0; i < int(std::size(kLanguages)); ++i)
        if (tag.compare(QLatin1String(kLanguages[i].tag), Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

void SettingsViewModel::setLanguageIndex(int index) {
    if (index < 0 || index >= int(std::size(kLanguages)))
        return;
    m_settings->setLanguage(QString::fromLatin1(kLanguages[index].tag));
}

bool SettingsViewModel::restartRequired() const {
    return m_settings->language().compare(m_startLanguage, Qt::CaseInsensitive) != 0;
}

bool SettingsViewModel::canResetPopularity() const {
    return m_service && m_service->isOpen() && !m_service->isReadOnly();
}

QString SettingsViewModel::applyPasswordLengthText(const QString &text) {
    QString digits;
    for (QChar c : text)
        if (c.isDigit() && c.unicode() < 128)
            digits += c;
    bool ok = false;
    const int value = digits.toInt(&ok);
    if (!digits.isEmpty() && !ok) // too many digits for an int
        m_settings->setPasswordLength(maxPasswordLength());
    else if (digits.isEmpty() || value == 0)
        m_settings->setPasswordLength(Core::AppSettings::Defaults::PasswordLength);
    else
        m_settings->setPasswordLength(std::clamp(value, minPasswordLength(), maxPasswordLength()));
    return digits;
}

void SettingsViewModel::applyPasswordCharsText(const QString &text) { m_settings->setPasswordChars(text); }

void SettingsViewModel::resetToDefaults() { m_settings->resetToDefaults(); }
void SettingsViewModel::resetPasswordChars() { m_settings->resetPasswordChars(); }
void SettingsViewModel::resetPasswordLength() { m_settings->resetPasswordLength(); }

bool SettingsViewModel::resetPopularity() {
    if (!canResetPopularity() || !m_service->resetPopularity()) {
        emit message(3, tr("Open a writable database to reset the popularity counter."));
        return false;
    }
    emit message(1, tr("The popularity counter was reset."));
    return true;
}

} // namespace EasePass::App
