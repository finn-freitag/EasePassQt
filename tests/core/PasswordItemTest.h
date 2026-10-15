#pragma once

#include <QObject>

class PasswordItemTest : public QObject {
    Q_OBJECT

private slots:
    void testJsonRoundtripFull();
    void testJsonMissingFieldsDefaults();
    void testJsonUnknownFieldsIgnored();
    void testHas2FA();
    void testTagsAndClicksSerialization();
    void testAvatarLetterDeterministic();
    void testAvatarColorDeterministic();
    void testAvatarTextColorContrast();
    void testRegisterClick();
};
