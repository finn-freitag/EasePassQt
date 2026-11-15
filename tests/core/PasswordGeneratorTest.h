#pragma once

#include <QObject>

class PasswordGeneratorTest : public QObject {
    Q_OBJECT

private slots:
    void testFromCharsetRespectsLengthAndChars();
    void testFromCharsetSecureAndTerminates();
};
