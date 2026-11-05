#pragma once
// Shared fakes and helpers for test_app (steps.txt T12). No network, real clipboard or real
// user dirs: everything is in memory.

#include "../core/FakeDatabaseSource.h"

#include "DatabaseFile.h"
#include "DatabaseService.h"
#include "IClipboard.h"
#include "IClock.h"
#include "INetworkClient.h"
#include "IScreenCapture.h"
#include "IWebcamScanner.h"
#include "LeakCheckService.h"
#include "PasswordItem.h"

#include <QSignalSpy>
#include <QTimer>

namespace AppTest {

// Test-only Argon2 parameters (steps.txt 3.3).
inline const EasePass::Core::KdfParams kFastKdf{1, 64, 1};
inline const char *kPassword = "correct horse";

class FakeClock : public EasePass::Core::IClock {
public:
    qint64 now = 1'700'000'000; // 20 s into a 30 s TOTP window
    qint64 currentSecsSinceEpoch() const override { return now; }
};

class FakeClipboard : public EasePass::Core::IClipboard {
public:
    QString content;
    QImage imageContent;
    bool lastSecret = false;
    int setCount = 0;
    QString text() const override { return content; }
    QImage image() const override { return imageContent; }
    void setText(const QString &text, bool secret) override {
        content = text;
        lastSecret = secret;
        ++setCount;
    }
    void clear() override { content.clear(); }
};

/// HIBP range client: answers every request with `body` (or holds them when deferred).
class FakeRangeClient : public EasePass::Core::INetworkClient {
public:
    QByteArray body;
    int statusCode = 200;
    bool deferred = false;
    int requests = 0;
    QList<ResponseCallback> pending;
    void get(const QNetworkRequest &, ResponseCallback callback) override {
        ++requests;
        if (deferred)
            pending.append(std::move(callback));
        else
            callback(response());
    }
    EasePass::Core::NetworkResponse response() const {
        EasePass::Core::NetworkResponse r;
        r.statusCode = statusCode;
        r.data = body;
        return r;
    }
    void release() {
        auto cbs = std::move(pending);
        pending.clear();
        for (auto &cb : cbs)
            cb(response());
    }
};

class FakeScreenCapture : public EasePass::Core::IScreenCapture {
public:
    EasePass::Core::ScreenCaptureResult result;
    bool available = true;
    QString name() const override { return QStringLiteral("fake"); }
    bool isAvailable() const override { return available; }
    void capture(Callback cb) override {
        auto r = result;
        QTimer::singleShot(0, [cb, r]() { cb(r); });
    }
};

/// Webcam without hardware: tests drive frames and detected codes by hand.
class FakeWebcamScanner : public EasePass::Core::IWebcamScanner {
public:
    QList<EasePass::Core::WebcamDevice> devices{{"cam0", QStringLiteral("Front"), false},
                                                 {"cam1", QStringLiteral("USB"), true}};
    bool activeFlag = false;
    QByteArray startedWith;
    int starts = 0;
    int stops = 0;
    QList<EasePass::Core::WebcamDevice> availableDevices() const override { return devices; }
    void start(const QByteArray &deviceId) override {
        startedWith = deviceId;
        ++starts;
        activeFlag = true;
        emit activeChanged(true);
    }
    void stop() override {
        ++stops;
        if (activeFlag) {
            activeFlag = false;
            emit activeChanged(false);
        }
    }
    bool isActive() const override { return activeFlag; }
    void detect(const QString &text) { emit codeDetected(text); }
    void frame(const QImage &img) { emit previewFrame(img); }
};

inline EasePass::Core::PasswordItem makeItem(const QString &name, const QString &password = QStringLiteral("pw"),
                                             const QStringList &tags = {}) {
    EasePass::Core::PasswordItem item;
    item.displayName = name;
    item.password = password;
    item.tags = tags;
    return item;
}

inline QByteArray makeDbBytes(const QList<EasePass::Core::PasswordItem> &items,
                              const EasePass::Core::SecretString &secondFactor = {}) {
    EasePass::Core::DatabaseFile db;
    db.masterPassword = EasePass::Core::SecretString(kPassword);
    db.secondFactor = secondFactor;
    db.useSecondFactor = !secondFactor.isEmpty();
    db.secondFactorType = secondFactor.isEmpty() ? 0 : 1;
    db.items = items;
    return db.serialize(kFastKdf);
}

inline std::shared_ptr<FakeDatabaseSource> makeSource(const QList<EasePass::Core::PasswordItem> &items) {
    return std::make_shared<FakeDatabaseSource>(makeDbBytes(items));
}

/// Opens @p items in @p svc and waits for the result.
inline bool openItems(EasePass::Core::DatabaseService &svc, const QList<EasePass::Core::PasswordItem> &items) {
    QSignalSpy spy(&svc, &EasePass::Core::DatabaseService::openFinished);
    if (!svc.open(makeSource(items), EasePass::Core::SecretString(kPassword)))
        return false;
    return spy.wait(30000) && svc.isOpen();
}

} // namespace AppTest
