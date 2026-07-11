cmake_minimum_required(VERSION 3.20)
project(runtime_benchmarks_userver)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_userver_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(

    # Performance tuning options
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

# find_library searches the directories on ENV{PATH}, which lets unrelated
# tool installs shadow system libraries. AMD uProf in particular ships its own
# ancient libicu*.so.56 in the bin directory it prepends to PATH, and
# userver's find_package(ICU) would otherwise link against it.
file(GLOB _amduprof_bin_dirs "/opt/AMDuProf*/bin")

if(_amduprof_bin_dirs)
    list(APPEND CMAKE_IGNORE_PATH ${_amduprof_bin_dirs})
endif()

set(USERVER_GIT_TAG v3.1)

if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(USERVER_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()

# Only userver-core (the coroutine engine) is needed. Missing third-party
# dependencies (libev, yaml-cpp, cctz, c-ares, cryptopp, fmt, ...) are
# downloaded and built automatically by userver's own find modules because
# USERVER_DOWNLOAD_PACKAGES defaults to ON.
CPMAddPackage(
    NAME userver
    GIT_REPOSITORY https://github.com/userver-framework/userver.git
    GIT_TAG ${USERVER_GIT_TAG}
    DOWNLOAD_ONLY YES)

# The patch lets userver's required chaotic-codegen venv be created with the
# standalone `virtualenv` tool on systems where `python3 -m venv` is
# unavailable (Debian without python3-venv); it is a no-op elsewhere. It is
# applied here (idempotently, guarded by a --dry-run) rather than through
# CPM's PATCHES keyword, whose generated patch step fails when it reruns
# against the already-patched source.
find_program(PATCH_EXECUTABLE patch)

if(PATCH_EXECUTABLE)
    set(USERVER_VENV_PATCH
        ${CMAKE_CURRENT_SOURCE_DIR}/patches/0001-venv-virtualenv-fallback.patch)
    execute_process(
        COMMAND ${PATCH_EXECUTABLE} -p1 --forward --dry-run -i ${USERVER_VENV_PATCH}
        WORKING_DIRECTORY ${userver_SOURCE_DIR}
        RESULT_VARIABLE USERVER_VENV_PATCH_APPLICABLE
        OUTPUT_QUIET ERROR_QUIET)

    if(USERVER_VENV_PATCH_APPLICABLE EQUAL 0)
        execute_process(
            COMMAND ${PATCH_EXECUTABLE} -p1 --forward -i ${USERVER_VENV_PATCH}
            WORKING_DIRECTORY ${userver_SOURCE_DIR}
            RESULT_VARIABLE USERVER_VENV_PATCH_RESULT)

        if(NOT USERVER_VENV_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${USERVER_VENV_PATCH}")
        endif()
    endif()
endif()

set(USERVER_FEATURE_UTEST OFF)
set(USERVER_FEATURE_TESTSUITE OFF)

# The benchmarks link the repo-standard malloc below instead
set(USERVER_FEATURE_JEMALLOC OFF)

# Skip the per-task stack accounting overhead
set(USERVER_FEATURE_STACK_USAGE_MONITOR OFF)

add_subdirectory(${userver_SOURCE_DIR} ${userver_BINARY_DIR})

userver_setup_environment()

# Since each new coroutine requires an allocation,
# they are sensitive to allocator performance.
# Any of tcmalloc, mimalloc, or jemalloc provide
# greatly superior performance to the default glibc malloc.
# Try to find any of these 3 before falling back to default.
find_package(libtcmalloc)

if(LIBTCMALLOC_FOUND)
    set(MALLOC_LIB "${LIBTCMALLOC_LIBRARY}")
    message(STATUS "Using malloc: ${MALLOC_LIB}")
else()
    find_package(libmimalloc)

    if(LIBMIMALLOC_FOUND)
        set(MALLOC_LIB "${LIBMIMALLOC_LIBRARY}")
        message(STATUS "Using malloc: ${MALLOC_LIB}")
    else()
        find_package(libjemalloc)

        if(LIBJEMALLOC_FOUND)
            set(MALLOC_LIB "${LIBJEMALLOC_LIBRARY}")
            message(STATUS "Using malloc: ${MALLOC_LIB}")
        else()
            message(STATUS "Using malloc: default")
        endif()
    endif()
endif()

link_libraries(${MALLOC_LIB} userver::core)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

add_executable(channel channel.cpp)

add_executable(io_socket_st io_socket_st.cpp)
