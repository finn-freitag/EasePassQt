// test_plugin: Wasmtime plugin host tests (steps.txt T26). Same runner as test_core:
// a function name argument runs only that function (one CTest test per function).
#include "PluginPocTest.h"

#include <QCoreApplication>
#include <QTest>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    PluginPocTest test;
    return QTest::qExec(&test, argc, argv);
}
