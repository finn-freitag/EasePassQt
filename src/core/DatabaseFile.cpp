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
#include <QUtf8StringView>
#include <openssl/crypto.h>

#include <cstring>

namespace EasePass::Core {

namespace {

// AES-CBC with a wrong key still passes the PKCS7 padding check about once in 256 tries
// (T24: made test_core.testImportPasswordsFromFile flaky). Such a plaintext is random bytes;
// real database content (even a damaged one) is UTF-8 JSON text. Random data of the size
// of a database practically always contains control bytes or invalid UTF-8.
bool looksLikeWrongKey(const char *data, qsizetype size) {
    for (qsizetype i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            return true;
        }
    }
    return !QUtf8StringView(data, size).isValidUtf8();
}

qsizetype bomLength(const char *data, qsizetype size) {
    return (size >= 3 && std::memcmp(data, "\xEF\xBB\xBF", 3) == 0) ? 3 : 0;
}

} // namespace

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
        // Random bytes = the padding check passed by chance with a wrong password.
        return looksLikeWrongKey(outerPlain.constData(), outerPlain.size()) ? LoadResult::WrongPassword
                                                                             : LoadResult::WrongFormat;
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

    // T23: the decrypted item JSON holds every password. It lives in a SecureBuffer so it is
    // locked in RAM and wiped on every return path (also on WrongFormat and with a BOM).
    // Keys in order: current associated data, then the "Database_Version_1,4" fallback (D5).
    // A key whose result is random bytes only passed the padding check by chance, so the
    // next key is still tried (T24).
    SecureBuffer innerPlain;
    QJsonDocument innerDoc;
    bool decrypted = false;
    for (const bool oldAssociatedData : {false, true}) {
        const SecureBuffer innerKey = CryptoHelper::deriveInnerKey(masterPassword, secondFactor, oldAssociatedData, kdf);
        if (!CryptoHelper::decryptAes(innerCipher, innerKey, innerPlain)) {
            continue;
        }
        // Skip a UTF-8 BOM without copying the plaintext (fromRawData does not copy).
        const qsizetype bom = bomLength(innerPlain.constData(), qsizetype(innerPlain.size()));
        const QByteArray innerJson =
            QByteArray::fromRawData(innerPlain.constData() + bom, qsizetype(innerPlain.size()) - bom);
        if (looksLikeWrongKey(innerJson.constData(), innerJson.size())) {
            continue;
        }
        decrypted = true;
        innerDoc = QJsonDocument::fromJson(innerJson, &parseError);
        break;
    }

    if (!decrypted) {
        return LoadResult::WrongPassword;
    }
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
