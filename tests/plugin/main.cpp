// test_plugin: plugin host tests (steps.txt T26/T27). Same runner as test_core: a
// function name argument runs only that function (one CTest test per function).
#include "PluginTests.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QTest>
#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char *argv[]) {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);

    std::vector<std::unique_ptr<QObject>> tests;
    tests.push_back(std::make_unique<PluginRuntimeTest>());
    tests.push_back(std::make_unique<PluginPackageTest>());
    tests.push_back(std::make_unique<PluginStorageTest>());
    tests.push_back(std::make_unique<PluginNetworkTest>());
    tests.push_back(std::make_unique<PluginManagerTest>());
    tests.push_back(std::make_unique<PluginAdaptersTest>());

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-functions") == 0) {
            for (const auto &t : tests)
                QTest::qExec(t.get(), argc, argv);
            return 0;
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            const QByteArray sig = QMetaObject::normalizedSignature(
                (QString::fromUtf8(argv[i]).section(':', 0, 0).toLatin1() + "()").constData());
            for (const auto &t : tests)
                if (t->metaObject()->indexOfSlot(sig.constData()) != -1)
                    return QTest::qExec(t.get(), argc, argv);
            std::cerr << "Test function not found: " << argv[i] << std::endl;
            return 1;
        }
    }
    int status = 0;
    for (const auto &t : tests)
        status |= QTest::qExec(t.get(), argc, argv);
    return status;
}
