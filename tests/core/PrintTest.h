#pragma once

#include <QObject>

// T10: print layout and PDF export.
class PrintTest : public QObject {
    Q_OBJECT

private slots:
    void testHtmlContainsFieldsAndEscapes();
    void testPasswordsExcludedByDefault();
    void testPasswordsIncludedWhenRequested();
    void testPdfHasPagesAndMetadata();
    void testLongListPaginates();
    void testEmptyListStillOnePage();
    void testPrintServiceExportPdfFile();
    void testPrintServiceExportPdfBadPath();
};
