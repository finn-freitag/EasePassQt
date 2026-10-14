# Test executables. Rules: steps.txt section 3.3.
#   - one executable per module, each registered with CTest
#   - labels: unit (fast), slow (full-cost Argon2: 250 MB x 10 iterations), integration
#   - tests run headless (QT_QPA_PLATFORM=offscreen) with no access to real user dirs

find_package(Qt6 ${EASEPASS_QT_MIN_VERSION} REQUIRED COMPONENTS Test)

set(EASEPASS_TEST_FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# easepass_add_qtest(<target> <test-name> LABELS <labels...> FUNCTIONS <functions...>)
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

# --- test_easepass (legacy combined core tests; split per module in T1) ------
add_executable(test_easepass test_easepass.cpp)

target_compile_definitions(test_easepass PRIVATE
    EASEPASS_TEST_FIXTURES_DIR="${EASEPASS_TEST_FIXTURES_DIR}"
)

target_link_libraries(test_easepass PRIVATE
    easepass_core
    Qt6::Test
    easepass_warnings
)

easepass_add_qtest(test_easepass
    LABELS slow
    FUNCTIONS
        testArgon2AndAes
        testDecryptCSharpDatabase
        testSaveAndLoadDatabaseRoundtrip
)

easepass_add_qtest(test_easepass
    LABELS unit
    FUNCTIONS
        testTotpGeneration
        testTotpUriParsing
        testPasswordGenerator
)

# Guard: fails if a test function exists in the executable but is not registered above.
add_test(NAME test_easepass.all_functions_registered
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:test_easepass>
        "-DREGISTERED=testArgon2AndAes;testDecryptCSharpDatabase;testSaveAndLoadDatabaseRoundtrip;testTotpGeneration;testTotpUriParsing;testPasswordGenerator"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckRegisteredTests.cmake
)
set_tests_properties(test_easepass.all_functions_registered PROPERTIES
    LABELS unit
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
