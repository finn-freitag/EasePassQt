# Launch test for the real `easepass` executable (steps.txt T24), offscreen.
#   -DAPP=<easepass> -DFIXTURE=<test.epdb> -DWORK_DIR=<scratch dir>
# 1. `easepass --version` prints the version and exits 0, without any display (no
#    QT_QPA_PLATFORM, DISPLAY or WAYLAND_DISPLAY: it must not need the GUI).
# 2. `easepass --db <copy of the fixture>` starts and keeps running (killed after 5 s); its output
#    must not contain QML errors/warnings or a crash. A private XDG profile keeps the real user
#    settings, logs and single-instance lock out of it. Only on Linux/BSD: on Windows and macOS
#    QStandardPaths ignores the XDG variables, so the run would use the real user profile.
foreach(var APP FIXTURE WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} not set")
    endif()
endforeach()

unset(ENV{QT_QPA_PLATFORM})
unset(ENV{DISPLAY})
unset(ENV{WAYLAND_DISPLAY})
execute_process(COMMAND "${APP}" --version
    OUTPUT_VARIABLE version_out ERROR_VARIABLE version_err RESULT_VARIABLE version_rc)
if(NOT version_rc EQUAL 0 OR NOT version_out MATCHES "^EasePass [0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "--version failed (${version_rc}): ${version_out}${version_err}")
endif()

if(CMAKE_HOST_WIN32 OR CMAKE_HOST_APPLE)
    message(STATUS "--version ok; offscreen run skipped (no private profile possible on this OS)")
    return()
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/config" "${WORK_DIR}/data" "${WORK_DIR}/cache" "${WORK_DIR}/runtime")
file(CHMOD "${WORK_DIR}/runtime" DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(COPY_FILE "${FIXTURE}" "${WORK_DIR}/test.epdb") # never touch the fixture itself
set(ENV{XDG_CONFIG_HOME} "${WORK_DIR}/config")
set(ENV{XDG_DATA_HOME} "${WORK_DIR}/data")
set(ENV{XDG_CACHE_HOME} "${WORK_DIR}/cache")
set(ENV{XDG_RUNTIME_DIR} "${WORK_DIR}/runtime")
set(ENV{QT_QPA_PLATFORM} "offscreen")

execute_process(COMMAND "${APP}" --db "${WORK_DIR}/test.epdb"
    TIMEOUT 5
    OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err RESULT_VARIABLE run_rc)
set(output "${run_out}${run_err}")
# Still running after 5 s is the expected outcome ("Process terminated due to timeout").
if(NOT run_rc MATCHES "timeout")
    message(FATAL_ERROR "easepass exited early (${run_rc}):\n${output}")
endif()
string(REGEX MATCHALL "[^\n]*(qrc:/|\\.qml:[0-9]|QML |ReferenceError|TypeError|Segmentation|Abort)[^\n]*" problems "${output}")
if(problems)
    string(REPLACE ";" "\n" problems "${problems}")
    message(FATAL_ERROR "QML problems in the app output:\n${problems}")
endif()
file(GLOB logs "${WORK_DIR}/data/*/*/logs/*.log" "${WORK_DIR}/data/*/logs/*.log")
if(NOT logs)
    message(FATAL_ERROR "no log file was written")
endif()
message(STATUS "easepass ran for 5 s without QML problems")
