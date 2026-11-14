#include "PluginTests.h"

#include "PluginStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace EasePass::Plugin;

void PluginStorageTest::testStrings() {
    QTemporaryDir root;
    PluginStorage storage(root.filePath(QStringLiteral("plugin")));
    std::optional<QString> value;

    QVERIFY(!storage.getString(QStringLiteral("missing"), &value));
    QVERIFY(!value.has_value());
    QVERIFY(!QFileInfo::exists(storage.directory())); // reading creates nothing

    QVERIFY(!storage.setString(QStringLiteral("k"), QString::fromUtf8("wert ✓")));
    QVERIFY(!storage.setString(QStringLiteral("empty"), QString()));
    QVERIFY(!storage.getString(QStringLiteral("k"), &value));
    QCOMPARE(*value, QString::fromUtf8("wert ✓"));
    QVERIFY(!storage.getString(QStringLiteral("empty"), &value));
    QVERIFY(value.has_value() && value->isEmpty());

    QStringList keys;
    QVERIFY(!storage.listKeys(&keys));
    QCOMPARE(keys, (QStringList{QStringLiteral("empty"), QStringLiteral("k")}));

    // A second object on the same directory sees the data (persisted).
    PluginStorage again(storage.directory());
    QVERIFY(!again.getString(QStringLiteral("k"), &value));
    QCOMPARE(*value, QString::fromUtf8("wert ✓"));

    QVERIFY(!storage.deleteString(QStringLiteral("k")));
    QVERIFY(!storage.deleteString(QStringLiteral("k"))); // deleting a missing key is fine
    QVERIFY(!storage.getString(QStringLiteral("k"), &value));
    QVERIFY(!value.has_value());

    QCOMPARE(storage.setString(QString(), QStringLiteral("v"))->code, QStringLiteral("invalid-argument"));
    QCOMPARE(storage.getString(QString(300, QLatin1Char('k')), &value)->code, QStringLiteral("invalid-argument"));
}

void PluginStorageTest::testFiles() {
    QTemporaryDir root;
    PluginStorage storage(root.filePath(QStringLiteral("plugin")));
    std::optional<QByteArray> data;

    QVERIFY(!storage.readFile(QStringLiteral("a.bin"), &data));
    QVERIFY(!data.has_value());

    const QByteArray binary("\x00\x01\xff\n", 4);
    QVERIFY(!storage.writeFile(QStringLiteral("a.bin"), binary));
    QVERIFY(!storage.writeFile(QString::fromUtf8("ünï code.txt"), "text"));
    QVERIFY(!storage.readFile(QStringLiteral("a.bin"), &data));
    QCOMPARE(*data, binary);
    QVERIFY(!storage.writeFile(QStringLiteral("a.bin"), "replaced"));
    QVERIFY(!storage.readFile(QStringLiteral("a.bin"), &data));
    QCOMPARE(*data, QByteArray("replaced"));

    QStringList names;
    QVERIFY(!storage.listFiles(&names));
    QCOMPARE(names.size(), 2);
    QVERIFY(names.contains(QStringLiteral("a.bin")));
    // Files live below <directory>/files and nowhere else.
    QVERIFY(QFileInfo::exists(QDir(storage.directory()).filePath(QStringLiteral("files/a.bin"))));

    QVERIFY(!storage.deleteFile(QStringLiteral("a.bin")));
    QVERIFY(!storage.deleteFile(QStringLiteral("a.bin")));
    QVERIFY(!storage.readFile(QStringLiteral("a.bin"), &data));
    QVERIFY(!data.has_value());

    QVERIFY(PluginStorage::removeAll(storage.directory()));
    QVERIFY(!QFileInfo::exists(storage.directory()));
    QVERIFY(PluginStorage::removeAll(storage.directory())); // nothing there: still fine
}

void PluginStorageTest::testInvalidNamesRejected_data() {
    QTest::addColumn<QString>("name");
    QTest::newRow("empty") << "";
    QTest::newRow("dot") << ".";
    QTest::newRow("dotdot") << "..";
    QTest::newRow("parent") << "../secret";
    QTest::newRow("deep parent") << "a/../../secret";
    QTest::newRow("subdirectory") << "sub/file";
    QTest::newRow("absolute") << "/etc/passwd";
    QTest::newRow("backslash") << "..\\secret";
    QTest::newRow("drive") << "C:evil";
    QTest::newRow("nul byte") << QString::fromLatin1("a\0b", 3);
    QTest::newRow("newline") << "a\nb";
    QTest::newRow("trailing dot") << "name.";
    QTest::newRow("trailing space") << "name ";
    QTest::newRow("device") << "NUL";
    QTest::newRow("device with extension") << "con.txt";
    QTest::newRow("too long") << QString(256, QLatin1Char('a'));
}

void PluginStorageTest::testInvalidNamesRejected() {
    QFETCH(QString, name);
    QTemporaryDir root;
    // A file next to the plugin directory that must stay untouched and unreadable.
    QFile secret(root.filePath(QStringLiteral("secret")));
    QVERIFY(secret.open(QIODevice::WriteOnly));
    secret.write("top secret");
    secret.close();
    PluginStorage storage(root.filePath(QStringLiteral("plugin")));
    QVERIFY(!storage.writeFile(QStringLiteral("ok"), "x"));

    QVERIFY(!PluginStorage::isValidFileName(name));
    std::optional<QByteArray> data;
    QCOMPARE(storage.readFile(name, &data)->code, QStringLiteral("invalid-argument"));
    QVERIFY(!data.has_value());
    QCOMPARE(storage.writeFile(name, "overwritten")->code, QStringLiteral("invalid-argument"));
    QCOMPARE(storage.deleteFile(name)->code, QStringLiteral("invalid-argument"));

    QVERIFY(secret.open(QIODevice::ReadOnly));
    QCOMPARE(secret.readAll(), QByteArray("top secret"));
    QCOMPARE(QDir(root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 2);
}

void PluginStorageTest::testQuota() {
    QTemporaryDir root;
    PluginStorage::Quota quota;
    quota.totalBytes = 1000;
    quota.maxKeys = 3;
    quota.maxFiles = 2;
    quota.maxValueBytes = 600;
    PluginStorage storage(root.filePath(QStringLiteral("plugin")), quota);

    QCOMPARE(storage.writeFile(QStringLiteral("big"), QByteArray(601, 'x'))->code, QStringLiteral("too-large"));
    QCOMPARE(storage.setString(QStringLiteral("k"), QString(601, QLatin1Char('x')))->code, QStringLiteral("too-large"));

    QVERIFY(!storage.writeFile(QStringLiteral("a"), QByteArray(500, 'a')));
    QCOMPARE(storage.writeFile(QStringLiteral("b"), QByteArray(600, 'b'))->code, QStringLiteral("too-large"));
    QVERIFY(!storage.writeFile(QStringLiteral("b"), QByteArray(300, 'b')));
    // Replacing a file counts its new size only.
    QVERIFY(!storage.writeFile(QStringLiteral("a"), QByteArray(600, 'a')));
    QCOMPARE(storage.writeFile(QStringLiteral("c"), "c")->code, QStringLiteral("too-large")); // third file
    // Strings share the total quota with the files.
    QCOMPARE(storage.setString(QStringLiteral("k"), QString(200, QLatin1Char('x')))->code, QStringLiteral("too-large"));

    QVERIFY(!storage.deleteFile(QStringLiteral("a")));
    for (const char *key : {"k1", "k2", "k3"})
        QVERIFY(!storage.setString(QLatin1String(key), QStringLiteral("v")));
    QCOMPARE(storage.setString(QStringLiteral("k4"), QStringLiteral("v"))->code, QStringLiteral("too-large"));
    QVERIFY(!storage.setString(QStringLiteral("k1"), QStringLiteral("changed"))); // existing key is fine
}

void PluginStorageTest::testIsolationBetweenDirectories() {
    QTemporaryDir root;
    PluginStorage first(root.filePath(QStringLiteral("storage/com.example.first")));
    PluginStorage second(root.filePath(QStringLiteral("storage/com.example.second")));

    QVERIFY(!first.setString(QStringLiteral("token"), QStringLiteral("first secret")));
    QVERIFY(!first.writeFile(QStringLiteral("data"), "first data"));

    std::optional<QString> value;
    std::optional<QByteArray> data;
    QStringList names;
    QVERIFY(!second.getString(QStringLiteral("token"), &value));
    QVERIFY(!value.has_value());
    QVERIFY(!second.readFile(QStringLiteral("data"), &data));
    QVERIFY(!data.has_value());
    QVERIFY(!second.listFiles(&names));
    QVERIFY(names.isEmpty());
    QVERIFY(!second.listKeys(&names));
    QVERIFY(names.isEmpty());
    // No name reaches the neighbour.
    QVERIFY(second.readFile(QStringLiteral("../com.example.first/files/data"), &data).has_value());
    QVERIFY(second.readFile(QStringLiteral("..\\com.example.first\\files\\data"), &data).has_value());

    QVERIFY(!second.setString(QStringLiteral("token"), QStringLiteral("second secret")));
    QVERIFY(!first.getString(QStringLiteral("token"), &value));
    QCOMPARE(*value, QStringLiteral("first secret"));

    // Removing one plugin's storage leaves the other alone.
    QVERIFY(PluginStorage::removeAll(first.directory()));
    QVERIFY(!second.getString(QStringLiteral("token"), &value));
    QCOMPARE(*value, QStringLiteral("second secret"));
}

void PluginStorageTest::testFilesAreOwnerOnly() {
#ifdef Q_OS_UNIX
    QTemporaryDir root;
    PluginStorage storage(root.filePath(QStringLiteral("plugin")));
    QVERIFY(!storage.setString(QStringLiteral("k"), QStringLiteral("v")));
    QVERIFY(!storage.writeFile(QStringLiteral("f"), "data"));
    const QFileDevice::Permissions others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                            QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    for (const QString &path : {storage.directory(), QDir(storage.directory()).filePath(QStringLiteral("settings.json")),
                                QDir(storage.directory()).filePath(QStringLiteral("files")),
                                QDir(storage.directory()).filePath(QStringLiteral("files/f"))})
        QVERIFY2(!(QFileInfo(path).permissions() & others), qPrintable(path));
#else
    QSKIP("POSIX permissions only");
#endif
}

void PluginStorageTest::testDamagedSettings() {
    QTemporaryDir root;
    PluginStorage storage(root.filePath(QStringLiteral("plugin")));
    QVERIFY(!storage.setString(QStringLiteral("k"), QStringLiteral("v")));
    QFile file(QDir(storage.directory()).filePath(QStringLiteral("settings.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ broken");
    file.close();

    // Reported as an error; the damaged file is not silently replaced.
    std::optional<QString> value;
    QCOMPARE(storage.getString(QStringLiteral("k"), &value)->code, QStringLiteral("internal"));
    QCOMPARE(storage.setString(QStringLiteral("k2"), QStringLiteral("v"))->code, QStringLiteral("internal"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{ broken"));
}
