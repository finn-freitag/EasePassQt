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
    core/WebsiteIconManagerTest.cpp
    core/AppSettingsTest.cpp
    core/SecureMemoryTest.cpp
    core/LocalFileDatabaseSourceTest.cpp
    core/DeferredSaveManagerTest.cpp
    core/DatabaseServiceTest.cpp
    core/ItemLogicTest.cpp
    core/DatabaseOperationsTest.cpp
    core/ClipboardInactivityTest.cpp
    core/LeakCheckServiceTest.cpp
    core/ScreenScanTest.cpp
    core/PrintTest.cpp
)

set(TEST_CORE_HEADERS
    core/CryptoHelperTest.h
    core/DatabaseFileTest.h
    core/PasswordItemTest.h
    core/TotpHelperTest.h
    core/PasswordGeneratorTest.h
    core/QrCodeScannerTest.h
    core/WebsiteIconManagerTest.h
    core/AppSettingsTest.h
    core/SecureMemoryTest.h
    core/LocalFileDatabaseSourceTest.h
    core/DeferredSaveManagerTest.h
    core/DatabaseServiceTest.h
    core/FakeDatabaseSource.h
    core/ItemLogicTest.h
    core/DatabaseOperationsTest.h
    core/ClipboardInactivityTest.h
    core/LeakCheckServiceTest.h
    core/ScreenScanTest.h
    core/PrintTest.h
)

add_executable(test_core ${TEST_CORE_SOURCES} ${TEST_CORE_HEADERS})

target_compile_definitions(test_core PRIVATE
    EASEPASS_TEST_FIXTURES_DIR="${EASEPASS_TEST_FIXTURES_DIR}"
)

target_link_libraries(test_core PRIVATE
    easepass_core
    easepass_app
    Qt6::Test
    easepass_warnings
)

set(TEST_CORE_SLOW_FUNCTIONS
    testDeriveOuterKeyKnownVector
    testDeriveInnerKeyKnownVector
    testAssociatedDataOldVsNew
    testLoadCSharpTestDbFixture
    testLoadCSharpOfficialTestEpdbFixture
    testLoadWrongPassword
    testSaveAndLoadRoundtrip
    testSecondFactorHandling
    testOpenCSharpFixtureThroughService
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
    testWrongPasswordPassingPaddingCheck
    testWrongSecondFactorPassingPaddingCheck
    testOldAssociatedDataWhenNewKeyPassesPadding
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
    testFromCharsetRespectsLengthAndChars
    testFromCharsetSecureAndTerminates
    testDecodeSampleQrNormal
    testDecodeSampleQrRotated
    testDecodeSampleQrInverted
    testDecodeSampleQrNoisy
    testNonQrImageReturnsEmpty
    testNullImageAndMissingFileReturnsEmpty
    testClipboardOtpauthText
    testClipboardImageScanning
    testGenerateRoundTrip
    testGenerateEmptyAndScale
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
    testSecureBufferBasicAllocAndSize
    testSecureBufferZeroOnDestructionWithHook
    testSecureBufferZeroWithCustomAllocator
    testSecureBufferCopySemantics
    testSecureBufferMoveSemantics
    testSecureBufferResizeAndReserve
    testSecureBufferConstantTimeComparison
    testSecureBufferAppend
    testSecretStringBasicAndZeroOnDestruction
    testSecretStringCopyAndMove
    testSecretStringComparisonAndConversion
    testSecretStringToUtf16LE
    testCryptoHelperWithSecureBufferAndSecretString
    testDatabaseFileWipeSecrets
    testPlatformCoreDumpControl
    testFakeCoreDumpControlInjection
    testNameDescriptionAndAvailability
    testGetBytesMissingAndExisting
    testSaveCreatesFileAndBackup
    testAtomicSaveKeepsOldFileWhenWriteFails
    testAtomicSaveFailsWhenDirectoryMissing
    testForcedReadOnlyRejectsSave
    testReadOnlyFileIsReadOnly
    testLastModifiedAndChangedSignal
    testDebounceCoalescesRequests
    testFlushRunsImmediately
    testFlushWithoutPendingIsNoOp
    testRequestsDuringSaveRunOnceAfterwards
    testCancelPending
    testErrorReportedBySignal
    testSaveNowAndIdleSignal
    testSerializeLoadFromBytesRoundtrip
    testOpenSuccessIsAsync
    testOpenWrongPasswordAsync
    testOpenNeedsSecondFactor
    testOpenAvailabilityHandling
    testOpenWhileOpeningRejected
    testOpenNullSourceRejected
    testMutationSchedulesDeferredSave
    testConcurrentEditsCoalesceIntoOneSave
    testEditsDuringRunningSaveAreSavedAfterwards
    testFlushOnCloseThenLogout
    testReadOnlyRejectsMutation
    testSourceBecomingReadOnly
    testSaveFailureReportedAndRetried
    testLockWipesSecrets
    testCloseWhileOpeningDiscardsResult
    testDestructorFlushesPendingSave
    testRegisterClickPersistedOnClose
    testLocalFileSourceEndToEnd
    testCommonPasswordSequenceModes
    testSpecialCharClassification
    testPasswordStrengthKnownSamples
    testPasswordStrengthRepeatedChars
    testPasswordStrengthReuseCount
    testIsSecureWithSelectiveClasses
    testSortByName
    testSortByUsername
    testSortByNotes
    testSortByFirstTag
    testSortByWebsite
    testSortByPopularLastYear
    testSortByPopularLast30DaysWithClock
    testSortByPasswordStrength
    testSortStabilityAndUnicodeCase
    testClearOldClicksCacheAndResetPopularity
    testFreeTextSearchAllFields
    testTagSearchSyntaxAndSuggestions
    testTagSearchNoResultsFound
    testFindItemsByNameAndTag
    testEmailAddressIndexer
    testPasswordItemTagHelper
    testSecondFactorTypesAndTokenGenerator
    testSanitizeAndBuildDatabasePath
    testValidateNewPasswordAndCreate
    testCreateDatabase
    testCreateDatabaseWithSecondFactor
    testCreateDatabaseNeverOverwrites
    testRenameDatabase
    testRenameRejectsLoadedAndExisting
    testDeleteDatabaseRemovesFromKnownList
    testDeleteRejectsLastAndLoaded
    testImportDatabaseFile
    testReadItemsFromFileWrongPassword
    testChangeMasterPassword
    testChangeMasterPasswordValidation
    testChangeMasterPasswordSaveFailureRestoresOld
    testSetAndRemoveSecondFactor
    testSecondFactorUnsupportedAndInvalid
    testRotateOneTimeSecondFactor
    testImportPasswordsMergeRules
    testImportPasswordsFromFile
    testExportDatabaseCopy
    testExportWithNewPasswordOpensWithNewPasswordOnly
    testExportSelectedItemsAndBackup
    testExportRejectsOpenDatabaseFile
    testForceSave
    testTemporaryDatabaseIsReadOnly
    testOperationsWithoutOpenDatabase
    testClipboardClearedAfterTimeout
    testClipboardNotClearedIfUserCopiedOther
    testClipboardRecopyRestartsTimer
    testClipboardDisabledWhenZero
    testClipboardSecretFlagAndEmptyText
    testClipboardClearOnDestruction
    testInactivityLogoutAfterTimeout
    testInactivityActivityResetsTimer
    testInactivityWarningThenLogout
    testInactivityWarningCancelledByActivity
    testInactivityDisabled
    testInactivityFollowsSettingChange
    testFaviconFetchDisabledBySetting
    testFaviconFetchDefaultOffWithoutSettings
    testSha1HexAndSuffixParsing
    testLeakedPassword
    testNotLeakedPassword
    testOnlyPrefixIsSent
    testNetworkErrorGivesUnknown
    testMalformedResponseGivesUnknown
    testCacheHitAvoidsRequest
    testCacheExpires
    testConcurrentChecksShareRequest
    testDisabledBySettingNoRequest
    testEmptyPasswordUnknown
    testLogicalToDeviceDprScales
    testUnionGeometryNegativeOrigin
    testMapSelectionToImage
    testMapSelectionClipsAndRejects
    testComposeScreensMixedDpr
    testChooseBackend
    testPortalRequestPath
    testPortalResponseParsing
    testScanDecision
    testScanImageAllAsync
    testScreenQrScannerFindsCode
    testScreenQrScannerNoCode
    testScreenQrScannerRegionOnScaledMultiMonitor
    testScreenQrScannerCaptureFailureAndCancel
    testScreenQrScannerUnavailableBackend
    testWebcamControllerDetectsAndThrottles
    testHtmlContainsFieldsAndEscapes
    testPasswordsExcludedByDefault
    testPasswordsIncludedWhenRequested
    testPdfHasPagesAndMetadata
    testLongListPaginates
    testEmptyListStillOnePage
    testPrintServiceExportPdfFile
    testPrintServiceExportPdfBadPath
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

# --- test_app (view models / services, steps.txt T12) --------------------------
add_executable(test_app
    app/main.cpp
    app/AppTests.h
    app/AppTestSupport.h
    app/ControllersTest.cpp
    app/PasswordModelsTest.cpp
    app/ItemViewModelTest.cpp
    app/AppServicesTest.cpp
    app/DailyTipServiceTest.cpp
    app/PasswordActionsTest.cpp
    app/ItemDialogsTest.cpp
    app/DatabaseManagementTest.cpp
    app/AppBehaviourTest.cpp
    app/SecurityTest.cpp
)
target_compile_definitions(test_app PRIVATE EASEPASS_TEST_FIXTURES_DIR="${EASEPASS_TEST_FIXTURES_DIR}")
target_link_libraries(test_app PRIVATE easepass_app Qt6::Test Qt6::Qml Qt6::Quick easepass_warnings)

set(TEST_APP_UNIT_FUNCTIONS
    testNavigationNeedsOpenDatabase
    testOpenAndLockResetNavigation
    testGoBackSkipsUnavailablePages
    testStartPageFirstRun
    testOpenSuccessBusyFlags
    testOpenWrongPasswordError
    testOpenNeedsSecondFactor
    testOpenEmptyPathError
    testLockAndSaveErrors
    testCreateDatabaseValidationAndSuccess
    testCreateAndOpen
    testImportDatabase
    testOneTimeSecondFactorRotatedAfterLogin
    testKnownDatabasesAndFirstRun
    testPathHelpers
    testParseTips
    testTipStableForADayAndChanges
    testLanguageFallbackAndMissingFiles
    testBundledTipFiles
    testListModelRolesAndNoSecrets
    testListModelResetsOnChanges
    testFilterFreeTextAndTags
    testFilterSignalsKeepViewsInSync
    testTagSuggestions
    testSortModesAndOrder
    testSourceIndexMapping
    testInPlaceEditKeepsRows
    testLoadEditCancel
    testCommitUpdatesService
    testNewItemCommitAndValidation
    testTotpValidationErrors
    testDeleteItem
    testSafetyChecksAndReuse
    testLeakCheckStates
    testLeakResultForStalePasswordIgnored
    testTotpSharedTimerTicks
    testApplyOtpauthUri
    testCopyActionsUseClipboardAndKeepClicks
    testUnloadOnLock
    testTotpTimerRefCount
    testDialogServiceRequestResponse
    testDialogServiceCancelAll
    testClipboardService
    testSettingsViewModel
    testSettingsLanguages
    testSettingsPasswordLengthText
    testSettingsResetPopularity
    testLanguageManager
    testAboutChangelogParser
    testAboutOpenUrl
    testAboutBuildInfo
    testPrintViewModelExportPdf
    testScreenScanViewModelFound
    testScreenScanViewModelFailureAndClear
    testAppContextQmlRegistration
    testCopyActions
    testTotpCode
    testRegisterClickAndReadOnly
    testDeleteItems
    testExportItems
    testExportWithNewPassword
    testWebsiteIconService
    testEditorTagsTextAndEmailSuggestions
    testPasswordReused
    testTwoFactorHelpers
    testDetailReloadsAfterEditorCommit
    testGeneratorRespectsSettings
    testGeneratorLeakRetries
    testGeneratorAsyncAndTypedPasswordCancels
    testQrImportFileAndClipboard
    testWebcamScanViewModel
    testDatabaseListStates
    testDeleteNeedsTypedName
    testRenameAndCopyPath
    testChangePasswordAndSecondFactor
    testExportSelectionAndNewPassword
    testImportIntoSelectedItems
    testSwitchToAndMakePermanent
    testSingleInstanceTwoClients
    testSingleInstanceHandover
    testFitToScreens
    testWindowGeometryRoundTrip
    testFileLoggerRotation
    testFileLoggerRedaction
    testAutoLogoutFlow
    testActivityEventFilter
    testCommandLine
    testStartupDatabase
    testNewDatabaseFilesOwnerOnly
    testPdfExportOwnerOnly
    testDecryptedPlaintextWiped
    testLockClearsClipboard
    testLockForgetsGeneratedPassword
    testCoreDumpLimitZero
    testLogFilesOwnerOnly
)
easepass_add_qtest(test_app LABELS unit FUNCTIONS ${TEST_APP_UNIT_FUNCTIONS})
add_test(NAME test_app.all_functions_registered
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:test_app>
        "-DREGISTERED=${TEST_APP_UNIT_FUNCTIONS}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckRegisteredTests.cmake
)
set_tests_properties(test_app.all_functions_registered PROPERTIES
    LABELS unit
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# --- test_qml (QML module, QtQuickTest; steps.txt T11) ------------------------
find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS QuickTest)
file(GLOB EASEPASS_QML_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_*.qml)

add_executable(test_qml
    qml/main.cpp
    qml/QmlTestContext.cpp
    qml/QmlTestContext.h
    ${CMAKE_SOURCE_DIR}/src/main/QuickStyle.cpp
    ${EASEPASS_QML_TESTS}
)
target_include_directories(test_qml PRIVATE ${CMAKE_SOURCE_DIR}/src/main)
target_link_libraries(test_qml PRIVATE Qt6::Test)
target_compile_definitions(test_qml PRIVATE
    EASEPASS_QML_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(test_qml PRIVATE
    easepass_app
    easepass_qml_module
    easepass_qml_moduleplugin
    Qt6::QuickTest
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    easepass_warnings
)

# Pin QML import and plugin paths to the Qt installation we built against, so stray
# QML2_IMPORT_PATH / NIXPKGS_QT6_QML_IMPORT_PATH / QT_PLUGIN_PATH entries from the desktop
# session (possibly another Qt version) cannot inject incompatible plugins. On Nix every Qt
# module has its own prefix, so collect the prefixes of the modules the tests need
# (Qt6<Module>_DIR = <prefix>/lib/cmake/Qt6<Module>).
set(EASEPASS_QML_TEST_ENV
    "QML2_IMPORT_PATH=unset:"
    "NIXPKGS_QT6_QML_IMPORT_PATH=unset:"
    "QML_IMPORT_PATH=reset:"
    "QT_PLUGIN_PATH=reset:")
foreach(_mod IN ITEMS Qml Svg)
    get_filename_component(_prefix "${Qt6${_mod}_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_prefix}/${QT6_INSTALL_QML}")
        list(APPEND EASEPASS_QML_TEST_ENV "QML_IMPORT_PATH=path_list_append:${_prefix}/${QT6_INSTALL_QML}")
    endif()
    if(EXISTS "${_prefix}/${QT6_INSTALL_PLUGINS}")
        list(APPEND EASEPASS_QML_TEST_ENV "QT_PLUGIN_PATH=path_list_append:${_prefix}/${QT6_INSTALL_PLUGINS}")
    endif()
endforeach()
list(REMOVE_DUPLICATES EASEPASS_QML_TEST_ENV)

# One CTest test per QML test file (new tst_*.qml files are picked up automatically).
foreach(qml_test IN LISTS EASEPASS_QML_TESTS)
    get_filename_component(qml_test_name ${qml_test} NAME_WE)
    add_test(NAME test_qml.${qml_test_name}
             COMMAND $<TARGET_FILE:test_qml> -input ${qml_test})
    set_tests_properties(test_qml.${qml_test_name} PROPERTIES
        LABELS unit
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "${EASEPASS_QML_TEST_ENV}"
    )
endforeach()

# test_app loads QML (AppContext registration test): pin the same Qt paths as test_qml.
foreach(fn IN LISTS TEST_APP_UNIT_FUNCTIONS)
    set_tests_properties(test_app.${fn} PROPERTIES ENVIRONMENT_MODIFICATION "${EASEPASS_QML_TEST_ENV}")
endforeach()

# --- Translations (T20) ----------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME i18n.resw2ts COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/test_resw2ts.py)
    # Every qsTr()/tr() text is in the committed .ts files (lupdate -no-obsolete on copies).
    add_test(NAME i18n.translations_up_to_date
        COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_translations.py
            --lupdate $<TARGET_FILE:Qt6::lupdate>
            --lconvert $<TARGET_FILE:Qt6::lconvert>
            --cmake ${CMAKE_COMMAND}
            --generator ${Qt6LinguistTools_DIR}/GenerateLUpdateProject.cmake
            --project-in ${PROJECT_BINARY_DIR}/.lupdate/${PROJECT_NAME}_lupdate_project.cmake
            --i18n ${PROJECT_SOURCE_DIR}/i18n)
    set_tests_properties(i18n.resw2ts i18n.translations_up_to_date PROPERTIES LABELS unit)
else()
    message(STATUS "Python 3 not found: translation checks are not registered")
endif()

# --- Application smoke tests (steps.txt T24) --------------------------------------
# test_smoke: real Main.qml + AppContext, unlocks a copy of the C# fixture (production Argon2).
add_executable(test_smoke
    smoke/SmokeTest.cpp
    ${CMAKE_SOURCE_DIR}/src/main/QuickStyle.cpp
)
target_include_directories(test_smoke PRIVATE ${CMAKE_SOURCE_DIR}/src/main)
target_compile_definitions(test_smoke PRIVATE EASEPASS_TEST_FIXTURES_DIR="${EASEPASS_TEST_FIXTURES_DIR}")
target_link_libraries(test_smoke PRIVATE
    easepass_app
    easepass_qml_module
    easepass_qml_moduleplugin
    Qt6::Test
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    easepass_warnings
)
add_test(NAME smoke.unlock_fixture_lists_items COMMAND $<TARGET_FILE:test_smoke>)
set_tests_properties(smoke.unlock_fixture_lists_items PROPERTIES
    LABELS "slow;smoke"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "${EASEPASS_QML_TEST_ENV}"
)

# The real executable: --version and a 5 s offscreen run without QML problems.
add_test(NAME smoke.launch_app
    COMMAND ${CMAKE_COMMAND}
        -DAPP=$<TARGET_FILE:easepass>
        -DFIXTURE=${EASEPASS_TEST_FIXTURES_DIR}/test.epdb
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/smoke_launch
        -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke/RunApp.cmake
)
set_tests_properties(smoke.launch_app PROPERTIES
    LABELS "unit;smoke"
    ENVIRONMENT_MODIFICATION "${EASEPASS_QML_TEST_ENV}"
)
