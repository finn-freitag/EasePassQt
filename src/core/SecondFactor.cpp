#include "SecondFactor.h"

#include <QRandomGenerator>
#include <array>

namespace EasePass::Core::SecondFactor {

SecondFactorType typeFromInt(int value) {
    switch (value) {
    case 1:
        return SecondFactorType::Token;
    case 2:
        return SecondFactorType::OTP;
    case 3:
        return SecondFactorType::Authenticator;
    default:
        return SecondFactorType::None;
    }
}

bool isSupported(SecondFactorType type) {
    return type == SecondFactorType::None || type == SecondFactorType::Token || type == SecondFactorType::OTP;
}

bool requiresToken(SecondFactorType type) {
    return type == SecondFactorType::Token || type == SecondFactorType::OTP;
}

bool isSecureToken(const SecretString &token, int length, int maxOccurrence) {
    if (static_cast<qsizetype>(token.size()) < length) {
        return false;
    }
    std::array<int, 256> counters{};
    const auto *p = reinterpret_cast<const unsigned char *>(token.constData());
    for (size_t i = 0; i < token.size(); ++i) {
        if (++counters[p[i]] > maxOccurrence) {
            counters.fill(0);
            return false;
        }
    }
    counters.fill(0);
    return true;
}

SecretString generateToken(int length) {
    if (length <= 0) {
        length = 1;
    }
    const int maxOccurrence = qMax(1, length / 3);
    QRandomGenerator *rng = QRandomGenerator::system(); // CSPRNG (C#: RandomNumberGenerator)

    SecureBuffer buffer;
    buffer.reserve(static_cast<size_t>(length));
    SecretString token;
    do {
        buffer.clear();
        buffer.reserve(static_cast<size_t>(length));
        for (int i = 0; i < length; ++i) {
            const char digit = static_cast<char>('0' + rng->bounded(10));
            buffer.append(&digit, 1);
        }
        token = SecretString(SecureBuffer(buffer));
    } while (!isSecureToken(token, length, maxOccurrence));
    buffer.clear();
    return token;
}

} // namespace EasePass::Core::SecondFactor
