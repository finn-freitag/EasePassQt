#include "QmlTestContext.h"

#include "AppContext.h"
#include "AppSettings.h"
#include "ClipboardService.h"
#include "DatabaseController.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "PasswordActions.h"
#include "PasswordFilterModel.h"
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

QmlTestContext::QmlTestContext(QObject *parent) : QObject(parent), m_clip(std::make_shared<AppTest::FakeClipboard>()) {
    AppContext::Deps deps;
    deps.settingsStore = std::make_shared<InMemorySettingsStore>();
    deps.clipboard = m_clip;
    deps.networkClient = std::make_shared<AppTest::FakeRangeClient>();
    deps.screenCapture = std::make_shared<AppTest::FakeScreenCapture>();
    deps.clock = &m_clock;
    deps.kdf = AppTest::kFastKdf;
    m_ctx = std::make_unique<AppContext>(deps);
    m_ctx->settings()->setVerifyNotLeaked(false); // no HIBP requests in UI tests
    connect(m_ctx->clipboardService(), &ClipboardService::copied, this, &QmlTestContext::clipboardChanged);
    connect(m_ctx->databaseService(), &DatabaseService::itemsChanged, this, &QmlTestContext::itemsChanged);
}

QmlTestContext::~QmlTestContext() = default;

QObject *QmlTestContext::listModel() const { return m_ctx->passwordFilterModel(); }
QObject *QmlTestContext::item() const { return m_ctx->itemViewModel(); }
QObject *QmlTestContext::actions() const { return m_ctx->passwordActions(); }
QObject *QmlTestContext::settings() const { return m_ctx->settings(); }
QObject *QmlTestContext::controller() const { return m_ctx->databaseController(); }
QObject *QmlTestContext::clipboard() const { return m_ctx->clipboardService(); }
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
