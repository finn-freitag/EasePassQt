# Fails if the QtTest executable TEST_EXE contains a test function that is not listed in
# REGISTERED. Prevents new test functions from silently never running under CTest.
# Usage: cmake -DTEST_EXE=<exe> -DREGISTERED=<a;b;c> -P CheckRegisteredTests.cmake

execute_process(
    COMMAND "${TEST_EXE}" -functions
    OUTPUT_VARIABLE out
    RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "'${TEST_EXE} -functions' failed with exit code ${rc}")
endif()

string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*\\(\\)" found "${out}")
set(missing "")
foreach(fn IN LISTS found)
    string(REPLACE "()" "" fn "${fn}")
    if(NOT fn IN_LIST REGISTERED)
        list(APPEND missing ${fn})
    endif()
endforeach()

if(missing)
    message(FATAL_ERROR "Test functions not registered with CTest in tests/CMakeLists.txt: ${missing}")
endif()
