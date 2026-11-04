// test_app: view model / service layer tests (steps.txt T12). Same runner as test_core:
// a function name argument runs only that function (one CTest test per function).
#include "AppTests.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTest>
#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char *argv[]) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv); // PrintService needs widgets (D7)

    std::vector<std::unique_ptr<QObject>> tests;
    tests.push_back(std::make_unique<AppControllerTest>());
    tests.push_back(std::make_unique<DatabaseControllerTest>());
    tests.push_back(std::make_unique<PasswordModelsTest>());
    tests.push_back(std::make_unique<ItemViewModelTest>());
    tests.push_back(std::make_unique<AppServicesTest>());

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
