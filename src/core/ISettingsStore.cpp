#include "ISettingsStore.h"
#include <QMutexLocker>

namespace EasePass::Core {

QSettingsStore::QSettingsStore(QString organization, QString application)
    : m_organization(std::move(organization)), m_application(std::move(application)), m_useFile(false) {}

QSettingsStore::QSettingsStore(QString fileName, QSettings::Format format)
    : m_fileName(std::move(fileName)), m_format(format), m_useFile(true) {}

QSettings QSettingsStore::createSettings() const {
    if (m_useFile) {
        return QSettings(m_fileName, m_format);
    }
    return QSettings(m_organization, m_application);
}

QVariant QSettingsStore::value(const QString &key, const QVariant &defaultValue) const {
    return createSettings().value(key, defaultValue);
}

void QSettingsStore::setValue(const QString &key, const QVariant &value) {
    createSettings().setValue(key, value);
}

bool QSettingsStore::contains(const QString &key) const {
    return createSettings().contains(key);
}

void QSettingsStore::remove(const QString &key) {
    createSettings().remove(key);
}

void QSettingsStore::sync() {
    createSettings().sync();
}

void QSettingsStore::clear() {
    createSettings().clear();
}

QStringList QSettingsStore::allKeys() const {
    return createSettings().allKeys();
}

// --- InMemorySettingsStore ---

QVariant InMemorySettingsStore::value(const QString &key, const QVariant &defaultValue) const {
    QMutexLocker locker(&m_mutex);
    auto it = m_values.constFind(key);
    if (it != m_values.constEnd()) {
        return it.value();
    }
    return defaultValue;
}

void InMemorySettingsStore::setValue(const QString &key, const QVariant &value) {
    QMutexLocker locker(&m_mutex);
    m_values.insert(key, value);
}

bool InMemorySettingsStore::contains(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_values.contains(key);
}

void InMemorySettingsStore::remove(const QString &key) {
    QMutexLocker locker(&m_mutex);
    m_values.remove(key);
}

void InMemorySettingsStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_values.clear();
}

QStringList InMemorySettingsStore::allKeys() const {
    QMutexLocker locker(&m_mutex);
    return m_values.keys();
}

} // namespace EasePass::Core
