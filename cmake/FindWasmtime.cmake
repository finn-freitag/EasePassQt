# Finds the Wasmtime C API (https://docs.wasmtime.dev/c-api/).
# Wasmtime ships neither a CMake package nor a pkg-config file, so look for the header and
# the library. Hint with -DWasmtime_ROOT=<dir containing include/ and lib/> (for example an
# unpacked wasmtime-v*-c-api release archive).
#
# Result: imported target Wasmtime::Wasmtime, Wasmtime_FOUND, Wasmtime_VERSION.

find_path(Wasmtime_INCLUDE_DIR NAMES wasmtime.h PATH_SUFFIXES include)
find_library(Wasmtime_LIBRARY NAMES wasmtime wasmtime.dll PATH_SUFFIXES lib)

if(Wasmtime_INCLUDE_DIR AND EXISTS "${Wasmtime_INCLUDE_DIR}/wasmtime.h")
    file(STRINGS "${Wasmtime_INCLUDE_DIR}/wasmtime.h" _wasmtime_version_line
         REGEX "#define WASMTIME_VERSION \"")
    string(REGEX REPLACE ".*\"([0-9.]+)\".*" "\\1" Wasmtime_VERSION "${_wasmtime_version_line}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Wasmtime
    REQUIRED_VARS Wasmtime_LIBRARY Wasmtime_INCLUDE_DIR
    VERSION_VAR Wasmtime_VERSION)

if(Wasmtime_FOUND AND NOT TARGET Wasmtime::Wasmtime)
    add_library(Wasmtime::Wasmtime UNKNOWN IMPORTED)
    set_target_properties(Wasmtime::Wasmtime PROPERTIES
        IMPORTED_LOCATION "${Wasmtime_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Wasmtime_INCLUDE_DIR}")
endif()

mark_as_advanced(Wasmtime_INCLUDE_DIR Wasmtime_LIBRARY)
