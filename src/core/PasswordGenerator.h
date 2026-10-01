#pragma once

#include <QString>

namespace EasePass::Core {

struct PasswordGeneratorOptions {
    int length = 16;
    bool useUpper = true;
    bool useLower = true;
    bool useDigits = true;
    bool useSymbols = true;
    QString customSymbols = "!@#$%^&*()-_=+[]{}|;:,.<>?";
};

class PasswordGenerator {
public:
    static QString generate(const PasswordGeneratorOptions& options = PasswordGeneratorOptions());
};

} // namespace EasePass::Core
