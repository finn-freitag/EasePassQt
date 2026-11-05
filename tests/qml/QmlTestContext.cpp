#include "QmlTestContext.h"

#include "AppContext.h"
#include "AppSettings.h"
#include "ClipboardService.h"
#include "DatabaseController.h"
#include "DatabaseManagementViewModel.h"
#include "DatabaseOperations.h"
#include "LocalFileDatabaseSource.h"
#include "PrintViewModel.h"
#include "AppController.h"
#include "SettingsViewModel.h"
#include "AboutViewModel.h"
#include "AutoLogoutService.h"
#include "InactivityManager.h"
#include <QFileInfo>
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "PasswordActions.h"
#include "PasswordFilterModel.h"
#include "PasswordGeneratorViewModel.h"
#include "QrCodeGenerator.h"
#include "QrImportService.h"
#include "ScreenScanViewModel.h"
#include "WebcamScanViewModel.h"

#include <QJsonObject>
#include <QPainter>
#include "TotpHelper.h"
#include "TotpTimerService.h"

using namespace EasePass::Core;
using namespace EasePass::App;

namespace {
const char *kSecret = "JBSWY3DPEHPK3PXP";

QList<PasswordItem> sampleItems() {
    // Chosen so every sort mode gives a different, known order.
    PasswordItem a = AppTest::makeItem("Banking", "Str0ng!Passw0rd-xyz", {"money"});
    a.username = "zoe";
    a.email = "zoe@bank.example";
    a.notes = "c notes";
    a.website = "https://bank.example";
    PasswordItem b = AppTest::makeItem("amazon", "weak", {"shop"});
    b.username = "alice";
    b.notes = "a notes";
    b.website = "https://c-amazon.example";
    b.secret = kSecret;
    PasswordItem c = AppTest::makeItem("Cloud", "medium123", {"backup"});
    c.username = "bob";
    c.notes = "b notes";
    c.website = "https://a-cloud.example";
    return {a, b, c};
}
} // namespace

QmlTestContext::QmlTestContext(QObject *parent)
    : QObject(parent), m_clip(std::make_shared<AppTest::FakeClipboard>()),
      m_screen(std::make_shared<AppTest::FakeScreenCapture>()),
      m_webcam(std::make_shared<AppTest::FakeWebcamScanner>()) {
    AppContext::Deps deps;
    deps.settingsStore = std::make_shared<InMemorySettingsStore>();
    deps.clipboard = m_clip;
    deps.networkClient = std::make_shared<AppTest::FakeRangeClient>();
    deps.screenCapture = m_screen;
    deps.webcamScanner = m_webcam;
    deps.clock = &m_clock;
    deps.kdf = AppTest::kFastKdf;
    m_ctx = std::make_unique<AppContext>(deps);
    m_ctx->settings()->setVerifyNotLeaked(false); // no HIBP requests in UI tests
    connect(m_ctx->clipboardService(), &ClipboardService::copied, this, &QmlTestContext::clipboardChanged);
    connect(m_ctx->databaseService(), &DatabaseService::itemsChanged, this, &QmlTestContext::itemsChanged);
    m_ctx->about()->setUrlOpener([this](const QUrl &url) {
        m_openedUrls << url.toString();
        emit openedUrlsChanged();
        return true;
    });
}

QmlTestContext::~QmlTestContext() = default;

QObject *QmlTestContext::listModel() const { return m_ctx->passwordFilterModel(); }
QObject *QmlTestContext::item() const { return m_ctx->itemViewModel(); }
QObject *QmlTestContext::actions() const { return m_ctx->passwordActions(); }
QObject *QmlTestContext::settings() const { return m_ctx->settings(); }
QObject *QmlTestContext::controller() const { return m_ctx->databaseController(); }
QObject *QmlTestContext::clipboard() const { return m_ctx->clipboardService(); }
QObject *QmlTestContext::editor() const { return m_ctx->editorViewModel(); }
QObject *QmlTestContext::generator() const { return m_ctx->passwordGenerator(); }
QObject *QmlTestContext::qrImport() const { return m_ctx->qrImport(); }
QObject *QmlTestContext::screenScan() const { return m_ctx->screenScanViewModel(); }
QObject *QmlTestContext::webcamScan() const { return m_ctx->webcamScan(); }
QObject *QmlTestContext::databaseManagement() const { return m_ctx->databaseManagement(); }
QObject *QmlTestContext::printer() const { return m_ctx->printViewModel(); }
QObject *QmlTestContext::appController() const { return m_ctx->appController(); }
QObject *QmlTestContext::settingsViewModel() const { return m_ctx->settingsViewModel(); }
QObject *QmlTestContext::about() const { return m_ctx->about(); }
QObject *QmlTestContext::autoLogout() const { return m_ctx->autoLogout(); }
void QmlTestContext::tickInactivity() { m_ctx->inactivityManager()->tick(); }

void QmlTestContext::clearOpenedUrls() {
    m_openedUrls.clear();
    emit openedUrlsChanged();
}

void QmlTestContext::clickAll() {
    DatabaseService *svc = m_ctx->databaseService();
    for (int i = 0; svc->isOpen() && i < svc->itemCount(); ++i)
        svc->registerClick(i, &m_clock);
}
int QmlTestContext::itemCount() const {
    return m_ctx->databaseService()->isOpen() ? m_ctx->databaseService()->itemCount() : 0;
}

bool QmlTestContext::openItems(const QList<PasswordItem> &items) {
    DatabaseService *svc = m_ctx->databaseService();
    svc->close();
    m_ctx->passwordFilterModel()->setFilterTextProgrammatically(QString());
    m_ctx->passwordFilterModel()->setSortMode(PasswordFilterModel::SortMode::None);
    m_ctx->passwordFilterModel()->setDescending(false);
    m_source = AppTest::makeSource(items);
    QSignalSpy spy(svc, &DatabaseService::openFinished);
    if (!svc->open(m_source, SecretString(AppTest::kPassword)))
        return false;
    const bool ok = spy.wait(30000) && svc->isOpen();
    emit itemsChanged();
    return ok;
}

bool QmlTestContext::openSample() { return openItems(sampleItems()); }
bool QmlTestContext::openEmpty() { return openItems({}); }

void QmlTestContext::setReadOnly(bool readOnly) {
    if (m_source)
        m_source->setReadOnly(readOnly);
}

void QmlTestContext::advanceClock(int seconds) {
    m_clock.now += seconds;
    m_ctx->totpTimer()->tickNow();
}

QString QmlTestContext::totpFor(qint64 when) const { return TotpHelper::generateToken(kSecret, when); }

void QmlTestContext::clearClipboard() {
    m_clip->content.clear();
    emit clipboardChanged();
}

int QmlTestContext::clicksOf(int index) const {
    const auto *svc = m_ctx->databaseService();
    return svc->isOpen() && index >= 0 && index < svc->itemCount() ? int(svc->items().at(index).clicks.size()) : -1;
}

QString QmlTestContext::nameOf(int index) const {
    const auto *svc = m_ctx->databaseService();
    return svc->isOpen() && index >= 0 && index < svc->itemCount() ? svc->items().at(index).displayName : QString();
}

QVariantMap QmlTestContext::itemAt(int index) const {
    const auto *svc = m_ctx->databaseService();
    if (!svc->isOpen() || index < 0 || index >= svc->itemCount())
        return {};
    return svc->items().at(index).toJson().toVariantMap();
}

void QmlTestContext::setClipboardText(const QString &text) {
    m_clip->content = text;
    m_clip->imageContent = QImage();
}

void QmlTestContext::setClipboardQr(const QString &text) {
    m_clip->content.clear();
    m_clip->imageContent = QrCodeGenerator::generate(text, 4);
}

void QmlTestContext::setScreenQrCodes(const QStringList &texts) {
    // Codes side by side on a white "desktop".
    QImage desktop(400 * std::max<qsizetype>(1, texts.size()), 400, QImage::Format_RGB32);
    desktop.fill(Qt::white);
    QPainter p(&desktop);
    for (qsizetype i = 0; i < texts.size(); ++i)
        p.drawImage(QPoint(int(i) * 400 + 20, 20), QrCodeGenerator::generate(texts.at(i), 4));
    p.end();
    ScreenCaptureResult r;
    r.image = desktop;
    r.logicalGeometry = desktop.rect();
    m_screen->result = r;
}

void QmlTestContext::webcamDetect(const QString &text) { m_webcam->detect(text); }
bool QmlTestContext::webcamActive() const { return m_webcam->isActive(); }

QString QmlTestContext::writeQrFile(const QString &text) {
    static int counter = 0;
    const QString path = m_tempDir.filePath(QStringLiteral("qr%1.png").arg(++counter));
    QrCodeGenerator::generate(text, 4).save(path);
    return QUrl::fromLocalFile(path).toString();
}

QString QmlTestContext::otpauthFor(const QString &secret, const QString &issuer) const {
    TotpParameters p;
    p.secret = secret;
    p.issuer = issuer;
    p.account = QStringLiteral("me@example.com");
    p.digits = 8;
    p.period = 60;
    p.algorithm = QStringLiteral("SHA256");
    return TotpHelper::generateOtpauthUri(p);
}

void QmlTestContext::setGeneratorSettings(int length, const QString &chars) {
    m_ctx->settings()->setPasswordLength(length);
    m_ctx->settings()->setPasswordChars(chars);
}

// --- Database files ---------------------------------------------------------------------------

QString QmlTestContext::tempPath(const QString &fileName) const { return m_tempDir.filePath(fileName); }

QString QmlTestContext::createSampleFile(const QString &name, const QString &secondFactorToken) {
    if (!openSample())
        return {};
    DatabaseService *svc = m_ctx->databaseService();
    const QString path = tempPath(name + DatabaseOperations::databaseExtension());
    QFile::remove(path);
    bool ok = false;
    if (!secondFactorToken.isEmpty()) {
        svc->setSecondFactor(SecondFactorType::Token, SecretString(secondFactorToken),
                             [](const DatabaseOpResult &) {});
        svc->waitForBackgroundWork();
    }
    m_ctx->databaseOperations()->backupDatabase(path, [&ok](const DatabaseOpResult &r) { ok = r.ok(); });
    m_ctx->databaseOperations()->waitForBackgroundWork();
    svc->close();
    svc->waitForBackgroundWork();
    if (!ok)
        return {};
    m_ctx->settings()->addDatabasePath(path);
    return path;
}

bool QmlTestContext::openFile(const QString &path, const QString &password, const QString &secondFactor) {
    DatabaseService *svc = m_ctx->databaseService();
    svc->close();
    svc->waitForBackgroundWork();
    QSignalSpy spy(svc, &DatabaseService::openFinished);
    const QString pw = password.isEmpty() ? QString::fromLatin1(AppTest::kPassword) : password;
    if (!svc->open(std::make_shared<LocalFileDatabaseSource>(path), SecretString(pw), SecretString(secondFactor)))
        return false;
    const bool ok = spy.wait(30000) && svc->isOpen();
    emit itemsChanged();
    return ok;
}

QStringList QmlTestContext::readNames(const QString &path, const QString &password, const QString &secondFactor) {
    QStringList names;
    m_ctx->databaseOperations()->readItemsFromFile(
        path, SecretString(password), SecretString(secondFactor),
        [&names](const DatabaseOpResult &r, const QList<PasswordItem> &items) {
            if (r.ok())
                for (const PasswordItem &i : items)
                    names << i.displayName;
        });
    m_ctx->databaseOperations()->waitForBackgroundWork();
    return names;
}

bool QmlTestContext::fileExists(const QString &path) const { return QFileInfo::exists(path); }
QStringList QmlTestContext::knownPaths() const { return m_ctx->settings()->databasePaths(); }
void QmlTestContext::setKnownPaths(const QStringList &paths) { m_ctx->settings()->setDatabasePaths(paths); }
QString QmlTestContext::password() const { return QString::fromLatin1(AppTest::kPassword); }

int QmlTestContext::secondFactorType() const {
    const auto *svc = m_ctx->databaseService();
    return svc->isOpen() && svc->usesSecondFactor() ? int(svc->secondFactorType()) : 0;
}

QString QmlTestContext::secondFactorToken() const {
    const auto *db = m_ctx->databaseService()->database();
    return db ? db->secondFactor.toQString() : QString();
}

void QmlTestContext::waitForBackground() {
    m_ctx->databaseOperations()->waitForBackgroundWork();
    m_ctx->databaseService()->waitForBackgroundWork();
}

bool QmlTestContext::openTemporary(const QString &path) {
    DatabaseService *svc = m_ctx->databaseService();
    svc->close();
    svc->waitForBackgroundWork();
    QSignalSpy spy(svc, &DatabaseService::openFinished);
    if (!m_ctx->databaseOperations()->openTemporaryDatabase(path, SecretString(AppTest::kPassword)))
        return false;
    const bool ok = spy.wait(30000) && svc->isOpen();
    emit itemsChanged();
    return ok;
}
