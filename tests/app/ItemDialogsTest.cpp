// T15: view-model side of the item dialogs (edit buffer extras, generator, QR import, webcam).
#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppSettings.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "PasswordGeneratorViewModel.h"
#include "QrCodeGenerator.h"
#include "QrCodeScanner.h"
#include "QrImportService.h"
#include "TotpHelper.h"
#include "TotpTimerService.h"
#include "WebcamScanViewModel.h"

#include <QQuickImageProvider>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

namespace {
const char *kSecret = "JBSWY3DPEHPK3PXP";

QList<PasswordItem> sample() {
    PasswordItem a = makeItem("Bank", "shared-pw", {"money", "work"});
    a.email = "zoe@bank.example";
    a.username = "zoe";
    PasswordItem b = makeItem("Mail", "shared-pw");
    b.email = "zoe@bank.example";
    PasswordItem c = makeItem("Cloud", "unique-pw");
    c.email = "bob@cloud.example";
    c.secret = kSecret;
    return {a, b, c};
}

struct Fixture {
    FakeClock clock;
    DatabaseService svc{kFastKdf};
    TotpTimerService timer{&clock};
    std::shared_ptr<AppSettings> settings = std::make_shared<AppSettings>(std::make_shared<InMemorySettingsStore>());
    ItemViewModel detail{&svc, &timer};
    ItemViewModel editor{&svc, &timer};

    Fixture() {
        detail.setSettings(settings.get());
        editor.setSettings(settings.get());
        if (!openItems(svc, sample()))
            qFatal("open failed");
    }
    ~Fixture() { svc.shutdown(); }
};

QString otpauth(const QString &secret, const QString &issuer = QStringLiteral("Bank")) {
    TotpParameters p;
    p.secret = secret;
    p.issuer = issuer;
    p.account = QStringLiteral("zoe");
    p.digits = 8;
    return TotpHelper::generateOtpauthUri(p);
}
} // namespace

void ItemDialogsTest::testEditorTagsTextAndEmailSuggestions() {
    Fixture f;
    QVERIFY(f.editor.load(0));
    QCOMPARE(f.editor.tagsText(), QStringLiteral("money work"));
    f.editor.setTagsText(QStringLiteral("  a  b a "));
    QCOMPARE(f.editor.tags(), QStringList({"a", "b", "a"})); // C#: ParseToTags (split on spaces)
    QVERIFY(f.editor.dirty());
    // Unique addresses of the whole database (C#: EmailAddressIndexer).
    QStringList mails = f.editor.emailSuggestions();
    mails.sort();
    QCOMPARE(mails, QStringList({"bob@cloud.example", "zoe@bank.example"}));
    f.editor.unload();
    QVERIFY(f.editor.emailSuggestions().isEmpty());
}

void ItemDialogsTest::testPasswordReused() {
    Fixture f;
    f.editor.newItem();
    QVERIFY(!f.editor.passwordReused());
    f.editor.setPassword(QStringLiteral("unique-pw"));
    QVERIFY(f.editor.passwordReused()); // used by Cloud
    f.editor.setPassword(QStringLiteral("fresh"));
    QVERIFY(!f.editor.passwordReused());
    // Editing Cloud: its own stored password does not count, Bank's shared one does.
    QVERIFY(f.editor.load(2));
    QVERIFY(!f.editor.passwordReused());
    f.editor.setPassword(QStringLiteral("shared-pw"));
    QVERIFY(f.editor.passwordReused());
    // Bank shares its password with Mail.
    QVERIFY(f.editor.load(0));
    QVERIFY(f.editor.passwordReused());
}

void ItemDialogsTest::testTwoFactorHelpers() {
    Fixture f;
    QVERIFY(f.editor.load(0));
    QVERIFY(f.editor.otpauthUri().isEmpty()); // no 2FA
    f.editor.setDigits(QString());
    f.editor.setInterval(QString());
    f.editor.setAlgorithm(QString());
    f.editor.prepare2FA();
    QCOMPARE(f.editor.digits(), QStringLiteral("6"));
    QCOMPARE(f.editor.interval(), QStringLiteral("30"));
    QCOMPARE(f.editor.algorithm(), QStringLiteral("SHA1"));
    f.editor.setDigits(QStringLiteral("8"));
    f.editor.prepare2FA(); // keeps typed values
    QCOMPARE(f.editor.digits(), QStringLiteral("8"));

    // Export (C#: TOTP.EncodeUrl): issuer = name, account = e-mail if set, else username.
    f.editor.setSecret(QString::fromLatin1(kSecret));
    TotpParameters p;
    QVERIFY(TotpHelper::parseOtpauthUri(f.editor.otpauthUri(), p));
    QCOMPARE(p.secret, QString::fromLatin1(kSecret));
    QCOMPARE(p.issuer, QStringLiteral("Bank"));
    QCOMPARE(p.account, QStringLiteral("zoe@bank.example"));
    QCOMPARE(p.digits, 8);
    QCOMPARE(p.period, 30);
    f.editor.setEmail(QString());
    QVERIFY(TotpHelper::parseOtpauthUri(f.editor.otpauthUri(), p));
    QCOMPARE(p.account, QStringLiteral("zoe"));

    // QR code data URL decodes back to the URI.
    const QString dataUrl = f.editor.otpauthQrCode();
    QVERIFY(dataUrl.startsWith(QLatin1String("data:image/png;base64,")));
    const QImage img = QImage::fromData(QByteArray::fromBase64(dataUrl.mid(dataUrl.indexOf(u',') + 1).toLatin1()), "PNG");
    QCOMPARE(QrCodeScanner::scanImage(img), f.editor.otpauthUri());

    f.editor.remove2FA();
    QVERIFY(!f.editor.has2FA());
    QVERIFY(f.editor.otpauthQrCode().isEmpty());
}

void ItemDialogsTest::testDetailReloadsAfterEditorCommit() {
    Fixture f;
    QVERIFY(f.detail.load(1));
    QVERIFY(f.editor.load(1));
    f.editor.setUsername(QStringLiteral("max"));
    QCOMPARE(f.detail.username(), QString()); // not saved yet
    QSignalSpy committed(&f.editor, &ItemViewModel::committed);
    QVERIFY(f.editor.commit());
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.at(0).at(0).toInt(), 1);
    QCOMPARE(f.svc.items().at(1).username, QStringLiteral("max"));
    QCOMPARE(f.detail.username(), QStringLiteral("max")); // detail pane follows
    QVERIFY(!f.detail.dirty());

    // A buffer with unsaved edits is left alone.
    f.detail.setNotes(QStringLiteral("typing"));
    QVERIFY(f.editor.load(1));
    f.editor.setUsername(QStringLiteral("other"));
    QVERIFY(f.editor.commit());
    QCOMPARE(f.detail.username(), QStringLiteral("max"));
    QCOMPARE(f.detail.notes(), QStringLiteral("typing"));

    // Item removed below the loaded index -> unloaded.
    f.detail.cancel();
    QVERIFY(f.detail.load(2));
    QVERIFY(f.svc.deleteItem(2));
    QVERIFY(!f.detail.hasItem());
}

void ItemDialogsTest::testGeneratorRespectsSettings() {
    Fixture f;
    f.settings->setVerifyNotLeaked(false);
    f.settings->setPasswordLength(24);
    f.settings->setPasswordChars(QStringLiteral("xyzXYZ0123#"));
    PasswordGeneratorViewModel gen(f.settings.get(), &f.svc, nullptr);
    QSignalSpy generated(&gen, &PasswordGeneratorViewModel::generated);
    gen.generate();
    QCOMPARE(generated.count(), 1); // synchronous without leak check
    const QString pw = gen.password();
    QCOMPARE(generated.at(0).at(0).toString(), pw);
    QCOMPARE(pw.length(), 24);
    for (QChar c : pw)
        QVERIFY2(QStringLiteral("xyzXYZ0123#").contains(c), qPrintable(pw));
    QVERIFY(!gen.busy());
    const QVariantList checks = gen.safetyChecks();
    QCOMPARE(checks.size(), 8);
    QCOMPARE(checks.at(2).toBool(), true);  // length
    QVERIFY(!checks.at(3).isValid());       // leak unknown
    QCOMPARE(checks.at(7).toBool(), true);  // not used by another item
    // Typed password: chart follows, a reused one is flagged.
    gen.setPassword(QStringLiteral("unique-pw"));
    QCOMPARE(gen.safetyChecks().at(7).toBool(), false);
    gen.setPassword(QString());
    QVERIFY(!gen.safetyChecks().at(0).isValid());
}

void ItemDialogsTest::testGeneratorLeakRetries() {
    Fixture f;
    f.settings->setVerifyNotLeaked(true);
    PasswordGeneratorViewModel gen(f.settings.get(), &f.svc, nullptr);
    // Leaked twice, then clean: three candidates.
    QStringList checked;
    gen.setLeakChecker([&](const QString &pw, std::function<void(PasswordGeneratorViewModel::LeakResult)> done) {
        checked << pw;
        done(checked.size() < 3 ? PasswordGeneratorViewModel::LeakResult::Leaked
                                : PasswordGeneratorViewModel::LeakResult::NotLeaked);
    });
    gen.generate();
    QCOMPARE(gen.lastAttempts(), 3);
    QCOMPARE(gen.password(), checked.at(2));
    QCOMPARE(gen.safetyChecks().at(3).toBool(), true);

    // Service unreachable (C#: IsPwned() == null) -> accepted at once.
    checked.clear();
    gen.setLeakChecker([&](const QString &pw, std::function<void(PasswordGeneratorViewModel::LeakResult)> done) {
        checked << pw;
        done(PasswordGeneratorViewModel::LeakResult::Unknown);
    });
    gen.generate();
    QCOMPARE(gen.lastAttempts(), 1);

    // Always leaked: gives up after MaxLeakRetries instead of looping forever.
    gen.setLeakChecker([](const QString &, std::function<void(PasswordGeneratorViewModel::LeakResult)> done) {
        done(PasswordGeneratorViewModel::LeakResult::Leaked);
    });
    gen.generate();
    QCOMPARE(gen.lastAttempts(), PasswordGeneratorViewModel::MaxLeakRetries);
    QVERIFY(!gen.busy());

    // Setting off -> no check at all.
    f.settings->setVerifyNotLeaked(false);
    int calls = 0;
    gen.setLeakChecker([&](const QString &, std::function<void(PasswordGeneratorViewModel::LeakResult)> done) {
        ++calls;
        done(PasswordGeneratorViewModel::LeakResult::NotLeaked);
    });
    gen.generate();
    QCOMPARE(calls, 0);
}

void ItemDialogsTest::testGeneratorAsyncAndTypedPasswordCancels() {
    Fixture f;
    f.settings->setVerifyNotLeaked(true);
    PasswordGeneratorViewModel gen(f.settings.get(), &f.svc, nullptr);
    std::function<void(PasswordGeneratorViewModel::LeakResult)> pending;
    gen.setLeakChecker([&](const QString &, std::function<void(PasswordGeneratorViewModel::LeakResult)> done) {
        pending = std::move(done);
    });
    QSignalSpy generated(&gen, &PasswordGeneratorViewModel::generated);
    gen.generate();
    QVERIFY(gen.busy());
    QVERIFY(gen.password().isEmpty());
    gen.generate(); // ignored while busy
    QCOMPARE(gen.lastAttempts(), 1);
    auto first = std::move(pending);
    gen.setPassword(QStringLiteral("typed")); // user typed meanwhile
    QVERIFY(!gen.busy());
    first(PasswordGeneratorViewModel::LeakResult::NotLeaked); // stale answer
    QCOMPARE(gen.password(), QStringLiteral("typed"));
    QCOMPARE(generated.count(), 0);
}

void ItemDialogsTest::testQrImportFileAndClipboard() {
    auto clip = std::make_shared<FakeClipboard>();
    QrImportService qr(clip);
    QSignalSpy found(&qr, &QrImportService::otpauthFound);
    QSignalSpy failed(&qr, &QrImportService::scanFailed);
    const QString uri = otpauth(QString::fromLatin1(kSecret));

    QTemporaryDir dir;
    const QString good = dir.filePath("good.png");
    const QString other = dir.filePath("other.png");
    QVERIFY(QrCodeGenerator::generate(uri, 4).save(good));
    QVERIFY(QrCodeGenerator::generate(QStringLiteral("https://example.com"), 4).save(other));

    QVERIFY(qr.scanFile(QUrl::fromLocalFile(good).toString())); // file:// URL from FileDialog
    QCOMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(0).toString(), uri);
    QVERIFY(qr.scanFile(good)); // plain path
    QVERIFY(!qr.scanFile(other)); // QR code, but not 2FA
    QVERIFY(!qr.scanFile(dir.filePath("missing.png")));
    QCOMPARE(failed.count(), 2);

    clip->content = QStringLiteral("  ") + uri + QStringLiteral("\n");
    QVERIFY(qr.scanClipboard());
    QCOMPARE(found.last().at(0).toString(), uri);
    clip->content.clear();
    clip->imageContent = QrCodeGenerator::generate(uri, 4);
    QVERIFY(qr.scanClipboard());
    clip->imageContent = QImage();
    clip->content = QStringLiteral("otpauth://totp/x?digits=6"); // no secret
    QVERIFY(!qr.scanClipboard());
    clip->content = QStringLiteral("hello");
    QVERIFY(!qr.scanClipboard());
    QCOMPARE(found.count(), 4);
    QCOMPARE(failed.count(), 4);

    QCOMPARE(QrImportService::describe(uri), QStringLiteral("Bank (zoe)"));
}

void ItemDialogsTest::testWebcamScanViewModel() {
    WebcamScanViewModel none(nullptr);
    QVERIFY(!none.available());
    none.start(); // no crash
    QVERIFY(!none.active());

    auto cam = std::make_shared<FakeWebcamScanner>();
    {
        WebcamScanViewModel vm(cam);
        QVERIFY(vm.available());
        QSignalSpy codes(&vm, &WebcamScanViewModel::codeDetected);
        vm.start();
        QCOMPARE(vm.deviceNames(), QStringList({"Front", "USB"}));
        QCOMPARE(vm.deviceIndex(), 1); // default device
        QCOMPARE(cam->startedWith, QByteArray("cam1"));
        QVERIFY(vm.active());
        vm.setDeviceIndex(0); // switching restarts on the new device
        QCOMPARE(cam->startedWith, QByteArray("cam0"));

        // Preview frames reach the image provider.
        std::unique_ptr<QQuickImageProvider> provider(vm.createImageProvider());
        QImage frame(32, 18, QImage::Format_RGB32);
        frame.fill(Qt::red);
        const int rev = vm.frameRevision();
        cam->frame(frame);
        QCOMPARE(vm.frameRevision(), rev + 1);
        QSize size;
        QCOMPARE(provider->requestImage(QString::number(vm.frameRevision()), &size, QSize()).pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(size, QSize(32, 18));

        // First code stops the camera and drops the last frame (may show the secret).
        cam->detect(QStringLiteral("otpauth://totp/x?secret=ABC"));
        QCOMPARE(codes.count(), 1);
        QVERIFY(!vm.active());
        QVERIFY(vm.currentFrame().isNull());

        cam->devices.clear();
        vm.start();
        QVERIFY(!vm.errorString().isEmpty());
        QVERIFY(!vm.active());
    }
    // Destroying the view model stops the camera.
    cam->devices = {{"cam0", QStringLiteral("Front"), true}};
    auto vm = std::make_unique<WebcamScanViewModel>(cam);
    vm->start();
    QVERIFY(cam->isActive());
    vm.reset();
    QVERIFY(!cam->isActive());
}
