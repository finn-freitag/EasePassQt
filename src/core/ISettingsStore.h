#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QHash>
#include <QMutex>
#include <QSettings>
#include <memory>

namespace EasePass::Core {

/**
 * Interface for settings storage.
 * Decoupled from QSettings to allow deterministic testing with in-memory stores (steps.txt section 3.1).
 */
class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;

    virtual QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const = 0;
    virtual void setValue(const QString &key, const QVariant &value) = 0;
    virtual bool contains(const QString &key) const = 0;
    virtual void remove(const QString &key) = 0;
    virtual void sync() = 0;
    virtual void clear() = 0;
    virtual QStringList allKeys() const = 0;
};

/**
 * QSettings-backed settings store.
 * Defaults to organization "EasePass" and application "EasePassQt",
 * respecting QSettings::setDefaultFormat and test paths.
 */
class QSettingsStore : public ISettingsStore {
public:
    explicit QSettingsStore(QString organization = QStringLiteral("EasePass"),
                            QString application = QStringLiteral("EasePassQt"));
    explicit QSettingsStore(QString fileName, QSettings::Format format);
    ~QSettingsStore() override = default;

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const override;
    void setValue(const QString &key, const QVariant &value) override;
    bool contains(const QString &key) const override;
    void remove(const QString &key) override;
    void sync() override;
    void clear() override;
    QStringList allKeys() const override;

private:
    QSettings createSettings() const;

    QString m_organization;
    QString m_application;
    QString m_fileName;
    QSettings::Format m_format{QSettings::NativeFormat};
    bool m_useFile{false};
};

/**
 * In-memory settings store for unit tests and hermetic execution.
 */
class InMemorySettingsStore : public ISettingsStore {
public:
    InMemorySettingsStore() = default;
    ~InMemorySettingsStore() override = default;

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const override;
    void setValue(const QString &key, const QVariant &value) override;
    bool contains(const QString &key) const override;
    void remove(const QString &key) override;
    void sync() override {}
    void clear() override;
    QStringList allKeys() const override;

private:
    mutable QMutex m_mutex;
    QHash<QString, QVariant> m_values;
};

} // namespace EasePass::Core
