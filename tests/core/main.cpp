#include <QGuiApplication>
#include <QTest>
#include <vector>
#include <memory>
#include <iostream>

#include "CryptoHelperTest.h"
#include "DatabaseFileTest.h"
#include "PasswordItemTest.h"
#include "TotpHelperTest.h"
#include "PasswordGeneratorTest.h"
#include "QrCodeScannerTest.h"
#include "DatabaseManagerTest.h"
#include "WebsiteIconManagerTest.h"
#include "AppSettingsTest.h"
#include "SecureMemoryTest.h"
#include "LocalFileDatabaseSourceTest.h"
#include "DeferredSaveManagerTest.h"
#include "DatabaseServiceTest.h"
#include "ItemLogicTest.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    std::vector<std::unique_ptr<QObject>> testObjects;
    testObjects.push_back(std::make_unique<CryptoHelperTest>());
    testObjects.push_back(std::make_unique<DatabaseFileTest>());
    testObjects.push_back(std::make_unique<PasswordItemTest>());
    testObjects.push_back(std::make_unique<TotpHelperTest>());
    testObjects.push_back(std::make_unique<PasswordGeneratorTest>());
    testObjects.push_back(std::make_unique<QrCodeScannerTest>());
    testObjects.push_back(std::make_unique<DatabaseManagerTest>());
    testObjects.push_back(std::make_unique<WebsiteIconManagerTest>());
    testObjects.push_back(std::make_unique<AppSettingsTest>());
    testObjects.push_back(std::make_unique<SecureMemoryTest>());
    testObjects.push_back(std::make_unique<LocalFileDatabaseSourceTest>());
    testObjects.push_back(std::make_unique<DeferredSaveManagerTest>());
    testObjects.push_back(std::make_unique<DatabaseServiceTest>());
    testObjects.push_back(std::make_unique<ItemLogicTest>());

    // 1. If -functions is requested, print functions for every test object
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-functions") == 0) {
            for (const auto &obj : testObjects) {
                QTest::qExec(obj.get(), argc, argv);
            }
            return 0;
        }
    }

    // 2. If a specific function is requested, locate its test object
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            QString targetFn = QString::fromUtf8(argv[i]).section(':', 0, 0);
            QByteArray slotSig = targetFn.toLatin1() + "()";
            QByteArray normalized = QMetaObject::normalizedSignature(slotSig.constData());

            for (const auto &obj : testObjects) {
                if (obj->metaObject()->indexOfSlot(normalized.constData()) != -1) {
                    return QTest::qExec(obj.get(), argc, argv);
                }
            }

            std::cerr << "Test function not found: " << argv[i] << std::endl;
            return 1;
        }
    }

    // 3. Otherwise run all test suites
    int status = 0;
    for (const auto &obj : testObjects) {
        status |= QTest::qExec(obj.get(), argc, argv);
    }
    return status;
}
