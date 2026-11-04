#include "ScreenScanViewModel.h"

namespace EasePass::App {

using Core::ScreenQrScanner;

ScreenScanViewModel::ScreenScanViewModel(std::shared_ptr<Core::IScreenCapture> capture, QObject *parent)
    : QObject(parent), m_capture(capture), m_scanner(new ScreenQrScanner(std::move(capture), this)) {
    connect(m_scanner, &ScreenQrScanner::finished, this, &ScreenScanViewModel::onFinished);
}

bool ScreenScanViewModel::start() {
    if (busy() || m_scanner->isBusy())
        return false;
    m_resultText.clear();
    m_candidates.clear();
    m_errorString.clear();
    m_status = Status::Scanning;
    emit stateChanged();
    m_scanner->start();
    return true;
}

bool ScreenScanViewModel::scanRegion(const QRect &selectionLogical) {
    if (busy() || m_scanner->isBusy() || m_scanner->lastCapture().image.isNull())
        return false;
    m_status = Status::Scanning;
    emit stateChanged();
    m_scanner->scanRegion(selectionLogical);
    return true;
}

bool ScreenScanViewModel::chooseCandidate(int index) {
    if (m_status != Status::Multiple || index < 0 || index >= m_candidates.size())
        return false;
    setFound(m_candidates.at(index));
    emit scanFinished(m_status);
    return true;
}

void ScreenScanViewModel::clear() {
    m_scanner->clear();
    m_status = Status::Idle;
    m_resultText.clear();
    m_candidates.clear();
    m_errorString.clear();
    emit stateChanged();
}

void ScreenScanViewModel::setFound(const QString &text) {
    m_status = Status::Found;
    m_resultText = text;
    m_candidates.clear();
    emit stateChanged();
    if (text.startsWith(QLatin1String("otpauth://"), Qt::CaseInsensitive))
        emit otpauthFound(text);
}

void ScreenScanViewModel::onFinished(const ScreenQrScanner::Outcome &outcome) {
    switch (outcome.status) {
    case ScreenQrScanner::Status::Found:
        setFound(outcome.text);
        break;
    case ScreenQrScanner::Status::Multiple:
        m_status = Status::Multiple;
        m_candidates = outcome.candidates;
        emit stateChanged();
        break;
    case ScreenQrScanner::Status::NoneFound:
        m_status = Status::NoneFound;
        emit stateChanged();
        break;
    case ScreenQrScanner::Status::Cancelled:
        m_status = Status::Cancelled;
        emit stateChanged();
        break;
    case ScreenQrScanner::Status::Busy:
        return; // the running scan will report
    case ScreenQrScanner::Status::Failed:
        m_status = Status::Failed;
        m_errorString = outcome.errorString;
        emit stateChanged();
        break;
    }
    emit scanFinished(m_status);
}

} // namespace EasePass::App
