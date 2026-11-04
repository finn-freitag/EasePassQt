#pragma once

#include "SecretString.h"

namespace EasePass::Core {

/**
 * @brief Second factor kinds stored in the database settings ("SecondFactorType" in the outer
 *        JSON, DatabaseFile::secondFactorType). Values are part of the file format.
 *
 * C#: Core/Database/Enums/SecondFactorType.cs, Views/DialogPages/ManageSecondFactorPage.xaml(.cs),
 * Views/LoginPage.xaml.cs (TryLogin).
 *  - None:  no second factor.
 *  - Token: a random 12-digit token is generated once (ManageSecondFactorPage), shown to the user
 *           (CopyTokenBox) and must be entered at every login.
 *  - OTP:   like Token, but after EVERY successful login a new 12-digit token is generated,
 *           shown once (ShowSecondFactorDialog) and the database is re-saved with it
 *           (DatabaseService::rotateOneTimeSecondFactor()).
 *  - Authenticator: declared in C# but never offered in the UI nor implemented; rejected here
 *           (DatabaseOpStatus::UnsupportedSecondFactorType).
 * In every case the token is mixed into the inner key (CryptoHelper::deriveInnerKey).
 */
enum class SecondFactorType : int {
    None = 0,
    Token = 1,
    OTP = 2,
    Authenticator = 3,
};

namespace SecondFactor {

/// Length of generated tokens (C#: TokenHelper.Generate(12)).
inline constexpr int DefaultTokenLength = 12;

/// Unknown values map to None.
SecondFactorType typeFromInt(int value);
/// true for the types this app can create (None, Token, OTP).
bool isSupported(SecondFactorType type);
/// true for Token and OTP.
bool requiresToken(SecondFactorType type);

/**
 * Generates a random numeric token of @p length digits (minimum 1) from the system CSPRNG.
 * Like C# TokenHelper.Generate, tokens are regenerated until no digit occurs more than
 * max(1, length / 3) times.
 */
SecretString generateToken(int length = DefaultTokenLength);

/// C#: TokenHelper.IsSecure. Checks length >= @p length and that no character occurs more than
/// @p maxOccurrence times.
bool isSecureToken(const SecretString &token, int length, int maxOccurrence);

} // namespace SecondFactor

} // namespace EasePass::Core
