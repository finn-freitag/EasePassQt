#include "DatabaseFile.h"
#include "CryptoHelper.h"
#include "FilePermissions.h"
#include "SortingHelper.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <openssl/crypto.h>

#include <cstring>

namespace EasePass::Core {

DatabaseFile::~DatabaseFile() {
    wipeSecrets();
}

void DatabaseFile::wipeSecrets() {
    masterPassword.clear();
    secondFactor.clear();
}

LoadResult DatabaseFile::loadFromFile(const QString &path, const SecretString &masterPassword, DatabaseFile &outDb,
                                      const SecretString &secondFactor) {
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return LoadResult::DatabaseNotFound;
    }

    QByteArray fileBytes = file.readAll();
    file.close();

    LoadResult res = loadFromBytes(fileBytes, masterPassword, outDb, secondFactor);
    if (res == LoadResult::Success || res == LoadResult::NeedsSecondFactor) {
        outDb.filePath = path;
        QFileInfo fileInfo(path);
        outDb.isReadOnly = !fileInfo.isWritable();
    }
    return res;
}

LoadResult DatabaseFile::loadFromBytes(const QByteArray &fileBytes, const SecretString &masterPassword,
                                       DatabaseFile &outDb, const SecretString &secondFactor, const KdfParams &kdf) {
    int versionTag = 0;
    QByteArray outerCipher;
    if (!CryptoHelper::extractVersionTag(fileBytes, versionTag, outerCipher)) {
        return LoadResult::WrongFormat;
    }

    if (versionTag != CryptoHelper::CURRENT_DB_VERSION_TAG) {
        return LoadResult::WrongFormat;
    }

    SecureBuffer outerKey = CryptoHelper::deriveOuterKey(masterPassword, kdf);
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
    outDb.masterPassword = masterPassword;
    outDb.databaseFileType = outerObj.value("DatabaseFileType").toInt(0);
    outDb.version = outerObj.value("Version").toDouble(1.4);

    QJsonObject settingsObj = outerObj.value("Settings").toObject();
    outDb.useSecondFactor = settingsObj.value("UseSecondFactor").toBool(false);
    outDb.secondFactorType = settingsObj.value("SecondFactorType").toInt(0);

    if (outDb.useSecondFactor && secondFactor.isEmpty()) {
        return LoadResult::NeedsSecondFactor;
    }

    outDb.secondFactor = secondFactor;

    QString dataBase64 = outerObj.value("Data").toString();
    QByteArray innerCipher = QByteArray::fromBase64(dataBase64.toUtf8());

    SecureBuffer innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, false, kdf);
    // T23: the decrypted item JSON holds every password. It lives in a SecureBuffer so it is
    // locked in RAM and wiped on every return path (also on WrongFormat and with a BOM).
    SecureBuffer innerPlain;
    bool decrypted = CryptoHelper::decryptAes(innerCipher, innerKey, innerPlain);

    if (!decrypted) {
        // Try backwards-compatibility associated data
        innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, true, kdf);
        decrypted = CryptoHelper::decryptAes(innerCipher, innerKey, innerPlain);
    }

    if (!decrypted) {
        return LoadResult::WrongPassword;
    }

    // Skip a UTF-8 BOM without copying the plaintext (fromRawData does not copy).
    const qsizetype bom = (innerPlain.size() >= 3 && std::memcmp(innerPlain.constData(), "\xEF\xBB\xBF", 3) == 0) ? 3 : 0;
    const QByteArray innerJson =
        QByteArray::fromRawData(innerPlain.constData() + bom, qsizetype(innerPlain.size()) - bom);
    QJsonDocument innerDoc = QJsonDocument::fromJson(innerJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !innerDoc.isArray()) {
        return LoadResult::WrongFormat;
    }

    outDb.items.clear();
    QJsonArray itemsArray = innerDoc.array();
    for (const auto &val : itemsArray) {
        if (val.isObject()) {
            outDb.items.append(PasswordItem::fromJson(val.toObject()));
        }
    }

    return LoadResult::Success;
}

bool DatabaseFile::saveToFile(const QString &path) {
    if (isReadOnly) {
        return false;
    }

    QString targetPath = path.isEmpty() ? filePath : path;
    if (targetPath.isEmpty()) {
        return false;
    }

    // 1.-5. Serialize, encrypt and add the version tag
    QByteArray finalBytes = serialize();
    if (finalBytes.isEmpty()) {
        return false;
    }

    // 6. Write atomically using QSaveFile
    QSaveFile saveFile(targetPath);
    const bool isNewFile = !QFile::exists(targetPath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (isNewFile) {
        restrictToOwner(saveFile); // T23: new database files are owner-only
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

QByteArray DatabaseFile::serialize(const KdfParams &kdf) const {
    // 1. Serialize items to JSON
    QJsonArray itemsArray;
    for (const auto &item : items) {
        itemsArray.append(item.toJson());
    }
    QJsonDocument innerDoc(itemsArray);
    QByteArray innerPlain = innerDoc.toJson(QJsonDocument::Indented);

    // 2. Encrypt inner JSON
    SecureBuffer innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, false, kdf);
    QByteArray innerCipher = CryptoHelper::encryptAes(innerPlain, innerKey);
    OPENSSL_cleanse(innerPlain.data(), static_cast<size_t>(innerPlain.size()));
    if (innerCipher.isEmpty()) {
        return QByteArray();
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
    SecureBuffer outerKey = CryptoHelper::deriveOuterKey(masterPassword, kdf);
    QByteArray outerCipher = CryptoHelper::encryptAes(outerPlain, outerKey);
    if (outerCipher.isEmpty()) {
        return QByteArray();
    }

    // 5. Add version tag
    return CryptoHelper::addVersionTag(outerCipher, CryptoHelper::CURRENT_DB_VERSION_TAG);
}

void DatabaseFile::addItem(const PasswordItem &item) {
    items.append(item);
}

void DatabaseFile::updateItem(int index, const PasswordItem &item) {
    if (index >= 0 && index < items.size()) {
        items[index] = item;
    }
}

void DatabaseFile::deleteItem(int index) {
    if (index >= 0 && index < items.size()) {
        items.removeAt(index);
    }
}

int DatabaseFile::countOccurrences(const QString &password) const {
    int count = 0;
    for (const auto &item : items) {
        if (item.password == password) {
            count++;
        }
    }
    return count;
}

void DatabaseFile::clearOldClicksCache(const IClock *clock) {
    SortingHelper::clearOldClicksCache(*this, clock);
}

void DatabaseFile::resetPopularity() {
    SortingHelper::resetPopularity(*this);
}

} // namespace EasePass::Core
