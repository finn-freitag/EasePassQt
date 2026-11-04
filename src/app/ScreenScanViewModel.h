#pragma once

#include "ScreenQrScanner.h"

#include <QObject>
#include <QRect>
#include <QStringList>

namespace EasePass::App {

/**
 * QR scanning from the screen for the 2FA dialog (steps.txt T12, T9). start() captures the
 * screen and scans it; one code -> Found, several -> Multiple (user picks one via
 * chooseCandidate()), or the user selects a region and scanRegion() re-scans it.
 * A found otpauth:// URI is reported via otpauthFound (ItemViewModel::applyOtpauthUri).
 */
class ScreenScanViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(Status status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged)
    Q_PROPERTY(QStringList candidates READ candidates NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    enum class Status { Idle, Scanning, Found, NoneFound, Multiple, Cancelled, Failed };
    Q_ENUM(Status)

    ScreenScanViewModel(std::shared_ptr<Core::IScreenCapture> capture, QObject *parent = nullptr);

    bool busy() const { return m_status == Status::Scanning; }
    Status status() const { return m_status; }
    QString resultText() const { return m_resultText; }
    QStringList candidates() const { return m_candidates; }
    QString errorString() const { return m_errorString; }
    bool available() const { return m_capture && m_capture->isAvailable(); }

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool scanRegion(const QRect &selectionLogical);
    Q_INVOKABLE bool chooseCandidate(int index);
    Q_INVOKABLE void clear();

signals:
    void stateChanged();
    void scanFinished(EasePass::App::ScreenScanViewModel::Status status);
    void otpauthFound(const QString &uri);

private:
    void onFinished(const Core::ScreenQrScanner::Outcome &outcome);
    void setFound(const QString &text);

    std::shared_ptr<Core::IScreenCapture> m_capture;
    Core::ScreenQrScanner *m_scanner;
    Status m_status = Status::Idle;
    QString m_resultText;
    QStringList m_candidates;
    QString m_errorString;
};

} // namespace EasePass::App
