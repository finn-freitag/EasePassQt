# Test executables. Rules: steps.txt section 3.3.
#   - one executable per module, each registered with CTest
#   - labels: unit (fast), slow (full-cost Argon2: 250 MB x 10 iterations), integration
#   - tests run headless (QT_QPA_PLATFORM=offscreen) with no access to real user dirs

find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS Test)

set(EASEPASS_TEST_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# easepass_add_qtest(<target> LABELS <labels...> FUNCTIONS <functions...>)
# Registers one CTest test per QtTest function so every function can carry its own label.
function(easepass_add_qtest target)
    cmake_parse_arguments(ARG "" "" "LABELS;FUNCTIONS" ${ARGN})
    foreach(fn IN LISTS ARG_FUNCTIONS)
        set(test_name "${target}.${fn}")
        add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${target}> ${fn})
        set_tests_properties(${test_name} PROPERTIES
            LABELS "${ARG_LABELS}"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endforeach()
endfunction()

# --- test_core (Core module characterization test suite) ---------------------
set(TEST_CORE_SOURCES
    core/main.cpp
    core/CryptoHelperTest.cpp
    core/DatabaseFileTest.cpp
    core/PasswordItemTest.cpp
    core/TotpHelperTest.cpp
    core/PasswordGeneratorTest.cpp
    core/QrCodeScannerTest.cpp
    core/DatabaseManagerTest.cpp
    core/WebsiteIconManagerTest.cpp
    core/AppSettingsTest.cpp
)

set(TEST_CORE_HEADERS
    core/CryptoHelperTest.h
    core/DatabaseFileTest.h
    core/PasswordItemTest.h
    core/TotpHelperTest.h
    core/PasswordGeneratorTest.h
    core/QrCodeScannerTest.h
    core/DatabaseManagerTest.h
    core/WebsiteIconManagerTest.h
    core/AppSettingsTest.h
)

add_executable(test_core ${TEST_CORE_SOURCES} ${TEST_CORE_HEADERS})

target_compile_definitions(test_core PRIVATE
    EASEPASS_TEST_FIXTURES_DIR="${EASEPASS_TEST_FIXTURES_DIR}"
)

target_link_libraries(test_core PRIVATE
    easepass_core
    Qt6::Test
    easepass_warnings
)

# Compatibility alias for legacy target name
add_custom_target(test_easepass DEPENDS test_core)

set(TEST_CORE_SLOW_FUNCTIONS
    testDeriveOuterKeyKnownVector
    testDeriveInnerKeyKnownVector
    testAssociatedDataOldVsNew
    testLoadCSharpTestDbFixture
    testLoadCSharpOfficialTestEpdbFixture
    testLoadWrongPassword
    testSaveAndLoadRoundtrip
    testSecondFactorHandling
)

set(TEST_CORE_UNIT_FUNCTIONS
    testToUtf16LE
    testCustomToBase64
    testAesEncryptionDecryptionRoundtrip
    testAesWrongKeyFailsDecryption
    testAesCorruptedCiphertextFails
    testAesTruncatedCiphertextFails
    testVersionTagGood
    testVersionTagMalformedHeader
    testVersionTagTruncated
    testLoadDatabaseNotFound
    testLoadWrongFormatNotEpdb
    testLoadWrongFormatBadVersionTag
    testLoadWrongFormatInvalidOuterJson
    testLoadWrongFormatInvalidInnerJson
    testSaveReadOnlyFails
    testSaveEmptyPathFails
    testItemAddUpdateDeleteBounds
    testCountOccurrences
    testJsonRoundtripFull
    testJsonMissingFieldsDefaults
    testJsonUnknownFieldsIgnored
    testHas2FA
    testTagsAndClicksSerialization
    testAvatarLetterDeterministic
    testAvatarColorDeterministic
    testAvatarTextColorContrast
    testRegisterClick
    testRfc6238Sha1Vectors
    testRfc6238Sha256Vectors
    testRfc6238Sha512Vectors
    testDigitsAndInterval
    testBase32Decoding
    testRemainingSecondsAndProgress
    testRemainingSecondsWithExplicitZeroTimestamp
    testClockInjection
    testOtpauthUriParsingValid
    testOtpauthUriParsingOddCasing
    testOtpauthUriParsingUrlEncoded
    testOtpauthUriParsingMissingSecretFails
    testOtpauthUriRoundtrip
    testLengthBounds
    testEachEnabledClassPresent
    testSelectiveClasses
    testEmptyPoolFallback
    testCustomSymbolsOnly
    testDistributionSanity
    testDecodeSampleQrNormal
    testDecodeSampleQrRotated
    testDecodeSampleQrInverted
    testDecodeSampleQrNoisy
    testNonQrImageReturnsEmpty
    testNullImageAndMissingFileReturnsEmpty
    testClipboardOtpauthText
    testClipboardImageScanning
    testKnownDatabasePathsAddRemoveDedupe
    testLastUsedDatabasePath
    testSetAndCloseDatabaseSignals
    testUrlNormalization
    testCacheMissAndAsyncLoad
    testMemoryCacheHit
    testDiskCacheHit
    testFailureFallbackToGoogleFavicons
    testEmptyDomainHandling
    testDefaults
    testSetGetAndSignals
    testRangeClamping
    testWindowGeometry
    testDatabasePathsAddRemoveDedupe
    testPersistenceAcrossInstances
    testLegacyKeysMigration
    testResetToDefaults
    testDatabaseManagerUsesAppSettings
)

easepass_add_qtest(test_core
    LABELS slow
    FUNCTIONS ${TEST_CORE_SLOW_FUNCTIONS}
)

easepass_add_qtest(test_core
    LABELS unit
    FUNCTIONS ${TEST_CORE_UNIT_FUNCTIONS}
)

# Guard: fails if a test function exists in the executable but is not registered above.
set(ALL_REGISTERED_FUNCTIONS ${TEST_CORE_SLOW_FUNCTIONS} ${TEST_CORE_UNIT_FUNCTIONS})
string(REPLACE ";" "\\;" ALL_REGISTERED_ARG "${ALL_REGISTERED_FUNCTIONS}")

add_test(NAME test_core.all_functions_registered
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:test_core>
        "-DREGISTERED=${ALL_REGISTERED_FUNCTIONS}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckRegisteredTests.cmake
)
set_tests_properties(test_core.all_functions_registered PROPERTIES
    LABELS unit
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
