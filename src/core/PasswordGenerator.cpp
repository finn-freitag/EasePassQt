#include "PasswordGenerator.h"

#include <QRandomGenerator>
#include <vector>
#include <algorithm>

namespace EasePass::Core {

QString PasswordGenerator::generate(const PasswordGeneratorOptions& options) {
    static const QString UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const QString LOWER = "abcdefghijklmnopqrstuvwxyz";
    static const QString DIGITS = "0123456789";

    QString pool;
    std::vector<QString> mandatorySets;

    if (options.useUpper) {
        pool += UPPER;
        mandatorySets.push_back(UPPER);
    }
    if (options.useLower) {
        pool += LOWER;
        mandatorySets.push_back(LOWER);
    }
    if (options.useDigits) {
        pool += DIGITS;
        mandatorySets.push_back(DIGITS);
    }
    if (options.useSymbols && !options.customSymbols.isEmpty()) {
        pool += options.customSymbols;
        mandatorySets.push_back(options.customSymbols);
    }

    if (pool.isEmpty()) {
        pool = LOWER + DIGITS;
        mandatorySets.push_back(LOWER);
        mandatorySets.push_back(DIGITS);
    }

    int targetLen = std::max(static_cast<int>(mandatorySets.size()), options.length);
    auto* rng = QRandomGenerator::system();

    QString result;
    result.reserve(targetLen);

    // Pick one from each mandatory set
    for (const auto& set : mandatorySets) {
        quint32 idx = rng->bounded(static_cast<quint32>(set.length()));
        result.append(set[idx]);
    }

    // Fill the rest from pool
    while (result.length() < targetLen) {
        quint32 idx = rng->bounded(static_cast<quint32>(pool.length()));
        result.append(pool[idx]);
    }

    // Fisher-Yates shuffle
    for (int i = result.length() - 1; i > 0; --i) {
        quint32 j = rng->bounded(static_cast<quint32>(i + 1));
        std::swap(result[i], result[j]);
    }

    return result;
}

} // namespace EasePass::Core
