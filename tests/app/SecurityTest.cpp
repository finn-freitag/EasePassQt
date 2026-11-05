// T23 security review: one regression test per fixed finding (docs/SECURITY.md lists them).
#include "AppTestSupport.h"
#include "AppTests.h"

#include "AppContext.h"
#include "AppSettings.h"
#include "ClipboardService.h"
#include "CryptoHelper.h"
#include "DatabaseFile.h"
#include "FileLogger.h"
#include "ICoreDumpControl.h"
#include "ISettingsStore.h"
#include "ItemViewModel.h"
#include "LocalFileDatabaseSource.h"
#include "PasswordGeneratorViewModel.h"
#include "PrintService.h"
#include "SecureBuffer.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <cstdlib>
#include <cstring>
#include <map>

#if defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif

using namespace EasePass::Core;
using namespace EasePass::App;
using namespace AppTest;

namespace {

constexpr QFileDevice::Permissions kOwnerRw = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
constexpr QFileDevice::Permissions kOthersAny = QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                                | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                                | QFileDevice::WriteOther | QFileDevice::ExeOther;

// Unix permission bits only (QFileInfo::permissions also reports the User* flags).
bool ownerOnly(const QString &path) {
    const QFileDevice::Permissions p = QFileInfo(path).permissions();
    return (p & kOthersAny) == 0 && (p & kOwnerRw) == kOwnerRw;
}

AppContext::Deps fakeDeps(FakeClock *clock, std::shared_ptr<FakeClipboard> clipboard) {
    AppContext::Deps deps;
    deps.settingsStore = std::make_shared<InMemorySettingsStore>();
    deps.clipboard = std::move(clipboard);
    deps.networkClient = std::make_shared<FakeRangeClient>();
    deps.screenCapture = std::make_shared<FakeScreenCapture>();
    deps.clock = clock;
    deps.kdf = kFastKdf;
    return deps;
}

// Builds an epdb file by hand so the inner plaintext can be anything (BOM, not an array).
QByteArray dbWithInnerPlaintext(const QByteArray &innerPlain) {
    const SecretString password(kPassword);
    const QByteArray innerCipher =
        CryptoHelper::encryptAes(innerPlain, CryptoHelper::deriveInnerKey(password, SecretString(), false, kFastKdf));
    QJsonObject outer;
    outer["DatabaseFileType"] = 0;
    outer["Version"] = 1.4;
    outer["Settings"] = QJsonObject{{"UseSecondFactor", false}, {"SecondFactorType", 0}};
    outer["Data"] = QString::fromLatin1(innerCipher.toBase64());
    const QByteArray outerCipher = CryptoHelper::encryptAes(QJsonDocument(outer).toJson(QJsonDocument::Compact),
                                                            CryptoHelper::deriveOuterKey(password, kFastKdf));
    return CryptoHelper::addVersionTag(outerCipher);
}

/// Records SecureBuffer allocations while installed: the largest one and whether every
/// released buffer was zeroed.
struct AllocationProbe {
    static inline size_t largest = 0;
    static inline bool allZeroed = true;
    SecureBuffer::CustomAllocator alloc;
    AllocationProbe() {
        largest = 0;
        allZeroed = true;
        alloc.allocate = [](size_t size) -> void * {
            largest = std::max(largest, size);
            return std::malloc(size);
        };
        alloc.deallocate = [](void *ptr, size_t size) {
            const auto *b = static_cast<const unsigned char *>(ptr);
            for (size_t i = 0; i < size; ++i)
                if (b[i] != 0) {
                    allZeroed = false;
                    break;
                }
            std::free(ptr);
        };
        SecureBuffer::setCustomAllocator(&alloc);
    }
    ~AllocationProbe() { SecureBuffer::setCustomAllocator(nullptr); }
};

} // namespace

// Finding 1: new database files (and their atomic temp files) were created with
// 0666 & ~umask, i.e. readable by every local user for the usual umask 022.
void SecurityTest::testNewDatabaseFilesOwnerOnly() {
#if !defined(Q_OS_UNIX)
    QSKIP("Unix permission bits only");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("new.epdb");
    LocalFileDatabaseSource source(path);
    QVERIFY(source.saveBytes(makeDbBytes({makeItem("A")})));
    QVERIFY2(ownerOnly(path), qPrintable(QString::number(int(QFileInfo(path).permissions()), 16)));

    // Second save creates the .bak copy; it keeps the owner-only permissions.
    QVERIFY(source.saveBytes(makeDbBytes({makeItem("B")})));
    QVERIFY(ownerOnly(LocalFileDatabaseSource::backupPathFor(path)));

    // A file the user deliberately shares (e.g. 0640) keeps its permissions on save.
    const QString shared = dir.filePath("shared.epdb");
    LocalFileDatabaseSource sharedSource(shared);
    QVERIFY(sharedSource.saveBytes(makeDbBytes({})));
    QVERIFY(QFile::setPermissions(shared, kOwnerRw | QFileDevice::ReadGroup));
    QVERIFY(sharedSource.saveBytes(makeDbBytes({makeItem("C")})));
    QVERIFY(QFileInfo(shared).permissions() & QFileDevice::ReadGroup);

    // Exports go through the same path.
    FakeClock clock;
    AppContext ctx(fakeDeps(&clock, std::make_shared<FakeClipboard>()));
    QVERIFY(openItems(*ctx.databaseService(), {makeItem("X")}));
    const QString exported = dir.filePath("export.epdb");
    bool finished = false;
    ctx.databaseService()->exportDatabase(std::make_shared<LocalFileDatabaseSource>(exported), {},
                                          [&finished](const DatabaseOpResult &) { finished = true; });
    QTRY_VERIFY_WITH_TIMEOUT(finished, 30000);
    QVERIFY(QFile::exists(exported));
    QVERIFY(ownerOnly(exported));
#endif
}

// Finding 1b: a PDF export (optionally with plaintext passwords) was world-readable too.
void SecurityTest::testPdfExportOwnerOnly() {
#if !defined(Q_OS_UNIX)
    QSKIP("Unix permission bits only");
#else
    QTemporaryDir dir;
    const QString path = dir.filePath("passwords.pdf");
    PrintService service;
    PrintDocumentBuilder::Options options;
    options.includePasswords = true;
    QString err;
    QVERIFY2(service.exportPdf({makeItem("Bank", "pdf-secret")}, options, path, &err), qPrintable(err));
    QVERIFY(ownerOnly(path));
#endif
}

// Finding 2: the decrypted item JSON (all passwords) was a plain QByteArray. With a UTF-8 BOM
// the original buffer was copied by mid(3) and freed unwiped, and on WrongFormat the
// function returned before the wipe. It is now a SecureBuffer, wiped on every path.
void SecurityTest::testDecryptedPlaintextWiped() {
    const QByteArray filler(6000, 'n');
    QJsonArray items;
    items.append(makeItem("Big", "Inner-Secret-1").toJson());
    QJsonObject big = makeItem("Notes", "Inner-Secret-2").toJson();
    big["Notes"] = QString::fromLatin1(filler);
    items.append(big);
    const QByteArray json = QJsonDocument(items).toJson(QJsonDocument::Compact);

    struct Case {
        const char *name;
        QByteArray plain;
        LoadResult expected;
    } cases[] = {
        {"plain", json, LoadResult::Success},
        {"bom", QByteArray("\xEF\xBB\xBF") + json, LoadResult::Success},
        {"wrong format", "{\"Notes\":\"" + filler + "\",\"Password\":\"Inner-Secret-3\"}", LoadResult::WrongFormat},
    };
    for (const Case &c : cases) {
        const QByteArray file = dbWithInnerPlaintext(c.plain);
        DatabaseFile db;
        {
            AllocationProbe probe;
            const LoadResult r = DatabaseFile::loadFromBytes(file, SecretString(kPassword), db, SecretString(), kFastKdf);
            QVERIFY2(r == c.expected, c.name);
        }
        // The plaintext went through a SecureBuffer (keys and passwords are far smaller)...
        QVERIFY2(AllocationProbe::largest >= size_t(filler.size()), c.name);
        // ...and every SecureBuffer released during the load was zeroed first.
        QVERIFY2(AllocationProbe::allZeroed, c.name);
        if (c.expected == LoadResult::Success) {
            QCOMPARE(db.items.size(), 2);
            QCOMPARE(db.items.at(1).password, QStringLiteral("Inner-Secret-2"));
            QCOMPARE(db.items.at(1).notes.size(), filler.size());
        }
        db.items.clear();
    }

    // The SecureBuffer overload fails cleanly on a wrong key and on short input.
    SecureBuffer out("stale");
    QVERIFY(!CryptoHelper::decryptAes(QByteArray(8, 'x'), SecureBuffer(32, 'k'), out));
    QVERIFY(out.isEmpty());
    const QByteArray cipher = CryptoHelper::encryptAes(QByteArray("hello"), SecureBuffer(32, 'a'));
    QVERIFY(!CryptoHelper::decryptAes(cipher, SecureBuffer(32, 'b'), out) || out != QByteArray("hello"));
    QVERIFY(CryptoHelper::decryptAes(cipher, SecureBuffer(32, 'a'), out));
    QVERIFY(out == QByteArray("hello"));
}

// Finding 3: locking (by hand or by the inactivity logout) left a copied password on the
// clipboard until the clear timer ran out.
void SecurityTest::testLockClearsClipboard() {
    FakeClock clock;
    auto clipboard = std::make_shared<FakeClipboard>();
    AppContext ctx(fakeDeps(&clock, clipboard));
    QVERIFY(openItems(*ctx.databaseService(), {makeItem("Bank", "Clip-Secret")}));
    QVERIFY(ctx.itemViewModel()->load(0));
    ctx.itemViewModel()->copyPassword();
    QCOMPARE(clipboard->content, QStringLiteral("Clip-Secret"));
    ctx.databaseService()->lock();
    ctx.databaseService()->waitForBackgroundWork();
    QVERIFY(clipboard->content.isEmpty());
    QVERIFY(!ctx.clipboardService()->clearPending());

    // Something the user copied afterwards is not ours and stays.
    QVERIFY(openItems(*ctx.databaseService(), {makeItem("Bank", "Clip-Secret")}));
    QVERIFY(ctx.itemViewModel()->load(0));
    ctx.itemViewModel()->copyPassword();
    clipboard->content = QStringLiteral("user text");
    ctx.databaseService()->lock();
    ctx.databaseService()->waitForBackgroundWork();
    QCOMPARE(clipboard->content, QStringLiteral("user text"));
}

// Finding 4: the generator view model kept the last generated password after logout.
void SecurityTest::testLockForgetsGeneratedPassword() {
    FakeClock clock;
    AppContext ctx(fakeDeps(&clock, std::make_shared<FakeClipboard>()));
    ctx.settings()->setVerifyNotLeaked(false); // synchronous, no leak check
    QVERIFY(openItems(*ctx.databaseService(), {makeItem("A")}));
    PasswordGeneratorViewModel *generator = ctx.passwordGenerator();
    generator->generate();
    QVERIFY(!generator->password().isEmpty());
    ctx.databaseService()->lock();
    ctx.databaseService()->waitForBackgroundWork();
    QVERIFY(generator->password().isEmpty());
}

// Finding 5: core dumps were only disabled via Linux PR_SET_DUMPABLE; the core size limit
// stayed as inherited (and nothing applied on other Unix systems).
void SecurityTest::testCoreDumpLimitZero() {
#if !defined(Q_OS_UNIX)
    QSKIP("POSIX resource limits only");
#else
    struct rlimit limit {};
    QCOMPARE(getrlimit(RLIMIT_CORE, &limit), 0);
    if (limit.rlim_max != 0) { // make sure the test can see the change
        limit.rlim_cur = limit.rlim_max == RLIM_INFINITY ? 1 << 20 : limit.rlim_max;
        QCOMPARE(setrlimit(RLIMIT_CORE, &limit), 0);
    }
    PlatformCoreDumpControl control;
    QVERIFY(control.disableCoreDumps());
    QCOMPARE(getrlimit(RLIMIT_CORE, &limit), 0);
    QCOMPARE(limit.rlim_cur, rlim_t(0));
    QCOMPARE(limit.rlim_max, rlim_t(0));
#if defined(Q_OS_LINUX)
    QVERIFY(!control.isDumpable());
#endif
#endif
}

// Finding 6: log files were created with umask permissions and only then chmod-ed; the log
// directory was world-listable.
void SecurityTest::testLogFilesOwnerOnly() {
#if !defined(Q_OS_UNIX)
    QSKIP("Unix permission bits only");
#else
    QTemporaryDir dir;
    const QString logDir = dir.filePath("logs");
    FileLogger::Options o;
    o.directory = logDir;
    FileLogger logger(o);
    QVERIFY(logger.open());
    QVERIFY(ownerOnly(logger.currentFile()));
    const QFileDevice::Permissions dirPerms = QFileInfo(logDir).permissions();
    QCOMPARE(dirPerms & kOthersAny, QFileDevice::Permissions());
#endif
}
