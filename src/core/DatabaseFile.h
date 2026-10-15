#pragma once

#include <QString>
#include <QList>
#include "PasswordItem.h"
#include "SecretString.h"

namespace EasePass::Core {

enum class LoadResult { Success, WrongPassword, DatabaseNotFound, WrongFormat, NeedsSecondFactor, Error };

class DatabaseFile {
public:
    DatabaseFile() = default;
    ~DatabaseFile();

    QString filePath;
    SecretString masterPassword;
    SecretString secondFactor;
    int databaseFileType = 0; // epdb
    double version = 1.4;
    bool useSecondFactor = false;
    int secondFactorType = 0;
    bool isReadOnly = false;

    QList<PasswordItem> items;

    static LoadResult loadFromFile(const QString &path, const SecretString &masterPassword, DatabaseFile &outDb,
                                   const SecretString &secondFactor = SecretString());

    bool saveToFile(const QString &path = QString());

    void wipeSecrets();

    void addItem(const PasswordItem &item);
    void updateItem(int index, const PasswordItem &item);
    void deleteItem(int index);

    int countOccurrences(const QString &password) const;
};

} // namespace EasePass::Core
