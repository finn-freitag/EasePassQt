#include "SettingsViewModel.h"

#include "AppSettings.h"

namespace EasePass::App {

SettingsViewModel::SettingsViewModel(Core::AppSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    connect(m_settings, &Core::AppSettings::passwordCharsChanged, this, &SettingsViewModel::passwordCharsValidChanged);
}

// The generator needs at least two distinct characters to make sense.
bool SettingsViewModel::passwordCharsValid() const {
    const QString chars = m_settings->passwordChars();
    for (QChar c : chars)
        if (c != chars.front())
            return true;
    return false;
}

void SettingsViewModel::resetToDefaults() { m_settings->resetToDefaults(); }
void SettingsViewModel::resetPasswordChars() { m_settings->resetPasswordChars(); }
void SettingsViewModel::resetPasswordLength() { m_settings->resetPasswordLength(); }

} // namespace EasePass::App
