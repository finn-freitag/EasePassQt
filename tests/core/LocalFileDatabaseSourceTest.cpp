#include "LocalFileDatabaseSourceTest.h"
#include "LocalFileDatabaseSource.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Core;

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    return f.write(data) == data.size();
}

QByteArray readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return f.readAll();
}

// Simulates a disk-full / I/O error in the middle of writing the temporary file.
class FailingWriteSource : public LocalFileDatabaseSource {
public:
    using LocalFileDatabaseSource::LocalFileDatabaseSource;

protected:
    bool writeContents(QIODevice &device, const QByteArray &bytes) override {
        device.write(bytes.left(bytes.size() / 2));
        return false;
    }
};

} // namespace

void LocalFileDatabaseSourceTest::testNameDescriptionAndAvailability() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("my.db.epdb");

    LocalFileDatabaseSource empty(QString{});
    QCOMPARE(empty.availability(), IDatabaseSource::Availability::UnknownState);

    LocalFileDatabaseSource src(path);
    QCOMPARE(src.databaseName(), QString("my.db"));
    QCOMPARE(src.sourceDescription(), path);
    QCOMPARE(src.availability(), IDatabaseSource::Availability::Unavailable);

    QVERIFY(writeFile(path, "x"));
    QCOMPARE(src.availability(), IDatabaseSource::Availability::Available);

    // A directory is not a database
    LocalFileDatabaseSource dirSrc(dir.path());
    QCOMPARE(dirSrc.availability(), IDatabaseSource::Availability::Unavailable);
}

void LocalFileDatabaseSourceTest::testGetBytesMissingAndExisting() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    LocalFileDatabaseSource src(path);
    QVERIFY(!src.getBytes().has_value());

    QVERIFY(writeFile(path, QByteArray("\x00\x01payload", 9)));
    auto bytes = src.getBytes();
    QVERIFY(bytes.has_value());
    QCOMPARE(*bytes, QByteArray("\x00\x01payload", 9));
}

void LocalFileDatabaseSourceTest::testSaveCreatesFileAndBackup() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    const QString bak = LocalFileDatabaseSource::backupPathFor(path);
    LocalFileDatabaseSource src(path);

    QVERIFY(src.saveBytes("first"));
    QCOMPARE(readFile(path), QByteArray("first"));
    QVERIFY(!QFile::exists(bak)); // nothing to back up on the first save

    QVERIFY(src.saveBytes("second"));
    QCOMPARE(readFile(path), QByteArray("second"));
    QCOMPARE(readFile(bak), QByteArray("first"));

    QVERIFY(src.saveBytes("third"));
    QCOMPARE(readFile(path), QByteArray("third"));
    QCOMPARE(readFile(bak), QByteArray("second"));

    // No temporary files left behind
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 2);
}

void LocalFileDatabaseSourceTest::testAtomicSaveKeepsOldFileWhenWriteFails() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    QVERIFY(writeFile(path, "old content"));

    FailingWriteSource src(path);
    QSignalSpy changedSpy(&src, &IDatabaseSource::changed);
    QVERIFY(!src.saveBytes("new content that is never completely written"));

    QCOMPARE(readFile(path), QByteArray("old content"));
    QCOMPARE(changedSpy.count(), 0);
    // Only the original file: no temp file, and no backup since the save never got that far
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden), QStringList{"db.epdb"});
}

void LocalFileDatabaseSourceTest::testAtomicSaveFailsWhenDirectoryMissing() {
    QTemporaryDir dir;
    const QString path = dir.filePath("missing/sub/db.epdb");
    LocalFileDatabaseSource src(path);
    QVERIFY(!src.saveBytes("data"));
    QVERIFY(!QFile::exists(path));
}

void LocalFileDatabaseSourceTest::testForcedReadOnlyRejectsSave() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    QVERIFY(writeFile(path, "old"));

    LocalFileDatabaseSource src(path, true);
    QVERIFY(src.isReadOnly());
    QVERIFY(!src.saveBytes("new"));
    QCOMPARE(readFile(path), QByteArray("old"));

    QSignalSpy spy(&src, &IDatabaseSource::changed);
    src.setReadOnly(false);
    QCOMPARE(spy.count(), 1);
    src.setReadOnly(false); // unchanged => no signal
    QCOMPARE(spy.count(), 1);
    QVERIFY(!src.isReadOnly());
    QVERIFY(src.saveBytes("new"));
    QCOMPARE(readFile(path), QByteArray("new"));
}

void LocalFileDatabaseSourceTest::testReadOnlyFileIsReadOnly() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    QVERIFY(writeFile(path, "old"));
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));
    if (QFileInfo(path).isWritable()) {
        QSKIP("File stays writable (running as root?)");
    }
    LocalFileDatabaseSource src(path);
    QVERIFY(src.isReadOnly());
    QCOMPARE(src.availability(), IDatabaseSource::Availability::Available);
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    QVERIFY(!src.isReadOnly());
}

void LocalFileDatabaseSourceTest::testLastModifiedAndChangedSignal() {
    QTemporaryDir dir;
    const QString path = dir.filePath("db.epdb");
    LocalFileDatabaseSource src(path);
    QVERIFY(!src.lastModified().isValid());

    QSignalSpy spy(&src, &IDatabaseSource::changed);
    QVERIFY(src.saveBytes("data"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(src.lastModified().isValid());
}
