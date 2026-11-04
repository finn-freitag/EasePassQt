#pragma once

#include <QObject>
#include <QUrl>

namespace EasePass::Core { class DatabaseService; }

namespace EasePass::App {

class PrintService;

/**
 * Print / PDF export for QML (steps.txt T12, T10). Prints all items of the open database;
 * `includePasswords` defaults to false and QML must show plaintextWarning first.
 */
class PrintViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString plaintextWarning READ plaintextWarning CONSTANT)
    Q_PROPERTY(bool canPrint READ canPrint NOTIFY canPrintChanged)
public:
    PrintViewModel(Core::DatabaseService *service, PrintService *printService, QObject *parent = nullptr);

    QString lastError() const { return m_lastError; }
    static QString plaintextWarning();
    bool canPrint() const;

    /// @p target: local path or file:// URL (from a QML FileDialog).
    Q_INVOKABLE bool exportPdf(const QString &target, bool includePasswords = false);
    Q_INVOKABLE bool print(bool includePasswords = false);
    Q_INVOKABLE void preview(bool includePasswords = false);

signals:
    void lastErrorChanged();
    void canPrintChanged();

private:
    void setError(const QString &e);
    Core::DatabaseService *m_service;
    PrintService *m_print;
    QString m_lastError;
};

} // namespace EasePass::App
