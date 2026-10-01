#include "DatabaseFile.h"
#include "CryptoHelper.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

namespace EasePass::Core {

LoadResult DatabaseFile::loadFromFile(const QString& path,
                                      const QString& masterPassword,
                                      DatabaseFile& outDb,
                                      const QString& secondFactor) {
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return LoadResult::DatabaseNotFound;
    }

    QByteArray fileBytes = file.readAll();
    file.close();

    int versionTag = 0;
    QByteArray outerCipher;
    if (!CryptoHelper::extractVersionTag(fileBytes, versionTag, outerCipher)) {
        return LoadResult::WrongFormat;
    }

    if (versionTag != CryptoHelper::CURRENT_DB_VERSION_TAG) {
        return LoadResult::WrongFormat;
    }

    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPassword);
    QByteArray outerPlain;
    if (!CryptoHelper::decryptAes(outerCipher, outerKey, outerPlain)) {
        return LoadResult::WrongPassword;
    }

    if (outerPlain.startsWith("\xEF\xBB\xBF")) {
        outerPlain = outerPlain.mid(3);
    }

    QJsonParseError parseError;
    QJsonDocument outerDoc = QJsonDocument::fromJson(outerPlain, &parseError);
    if (parseError.error != QJsonParseError::NoError || !outerDoc.isObject()) {
        return LoadResult::WrongFormat;
    }

    QJsonObject outerObj = outerDoc.object();
    outDb.filePath = path;
    outDb.masterPassword = masterPassword;
    outDb.databaseFileType = outerObj.value("DatabaseFileType").toInt(0);
    outDb.version = outerObj.value("Version").toDouble(1.4);

    QJsonObject settingsObj = outerObj.value("Settings").toObject();
    outDb.useSecondFactor = settingsObj.value("UseSecondFactor").toBool(false);
    outDb.secondFactorType = settingsObj.value("SecondFactorType").toInt(0);

    QFileInfo fileInfo(path);
    outDb.isReadOnly = !fileInfo.isWritable();

    if (outDb.useSecondFactor && secondFactor.isEmpty()) {
        return LoadResult::NeedsSecondFactor;
    }

    outDb.secondFactor = secondFactor;

    QString dataBase64 = outerObj.value("Data").toString();
    QByteArray innerCipher = QByteArray::fromBase64(dataBase64.toUtf8());

    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, false);
    QByteArray innerPlain;
    bool decrypted = CryptoHelper::decryptAes(innerCipher, innerKey, innerPlain);

    if (!decrypted) {
        // Try backwards-compatibility associated data
        innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, true);
        decrypted = CryptoHelper::decryptAes(innerCipher, innerKey, innerPlain);
    }

    if (!decrypted) {
        return LoadResult::WrongPassword;
    }

    if (innerPlain.startsWith("\xEF\xBB\xBF")) {
        innerPlain = innerPlain.mid(3);
    }

    QJsonDocument innerDoc = QJsonDocument::fromJson(innerPlain, &parseError);
    if (parseError.error != QJsonParseError::NoError || !innerDoc.isArray()) {
        return LoadResult::WrongFormat;
    }

    outDb.items.clear();
    QJsonArray itemsArray = innerDoc.array();
    for (const auto& val : itemsArray) {
        if (val.isObject()) {
            outDb.items.append(PasswordItem::fromJson(val.toObject()));
        }
    }

    return LoadResult::Success;
}

bool DatabaseFile::saveToFile(const QString& path) {
    if (isReadOnly) {
        return false;
    }

    QString targetPath = path.isEmpty() ? filePath : path;
    if (targetPath.isEmpty()) {
        return false;
    }

    // 1. Serialize items to JSON
    QJsonArray itemsArray;
    for (const auto& item : items) {
        itemsArray.append(item.toJson());
    }
    QJsonDocument innerDoc(itemsArray);
    QByteArray innerPlain = innerDoc.toJson(QJsonDocument::Indented);

    // 2. Encrypt inner JSON
    QByteArray innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, false);
    QByteArray innerCipher = CryptoHelper::encryptAes(innerPlain, innerKey);
    if (innerCipher.isEmpty()) {
        return false;
    }

    // 3. Construct outer JSON
    QJsonObject settingsObj;
    settingsObj["UseSecondFactor"] = useSecondFactor;
    settingsObj["SecondFactorType"] = secondFactorType;

    QJsonObject outerObj;
    outerObj["DatabaseFileType"] = databaseFileType;
    outerObj["Version"] = version;
    outerObj["Settings"] = settingsObj;
    outerObj["Data"] = QString::fromUtf8(innerCipher.toBase64());

    QJsonDocument outerDoc(outerObj);
    QByteArray outerPlain = outerDoc.toJson(QJsonDocument::Compact);

    // 4. Encrypt outer JSON
    QByteArray outerKey = CryptoHelper::deriveOuterKey(masterPassword);
    QByteArray outerCipher = CryptoHelper::encryptAes(outerPlain, outerKey);
    if (outerCipher.isEmpty()) {
        return false;
    }

    // 5. Add version tag
    QByteArray finalBytes = CryptoHelper::addVersionTag(outerCipher, CryptoHelper::CURRENT_DB_VERSION_TAG);

    // 6. Write atomically using QSaveFile
    QSaveFile saveFile(targetPath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    if (saveFile.write(finalBytes) != finalBytes.size()) {
        saveFile.cancelWriting();
        return false;
    }

    if (!saveFile.commit()) {
        return false;
    }

    filePath = targetPath;
    return true;
}

void DatabaseFile::addItem(const PasswordItem& item) {
    items.append(item);
}

void DatabaseFile::updateItem(int index, const PasswordItem& item) {
    if (index >= 0 && index < items.size()) {
        items[index] = item;
    }
}

void DatabaseFile::deleteItem(int index) {
    if (index >= 0 && index < items.size()) {
        items.removeAt(index);
    }
}

int DatabaseFile::countOccurrences(const QString& password) const {
    int count = 0;
    for (const auto& item : items) {
        if (item.password == password) {
            count++;
        }
    }
    return count;
}

} // namespace EasePass::Core
