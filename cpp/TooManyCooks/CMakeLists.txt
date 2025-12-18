cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_TooManyCooks)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_TooManyCooks_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(

    # Performance tuning options
    "-march=native"
    "-DTMC_PRIORITY_COUNT=1" # unsigned integer between 1 and 63

    # "-DTMC_TRIVIAL_TASK" # enabled in this repo for Release builds via CMakePresets.json

    # Other
    # "-DTMC_WORK_ITEM=CORO" # one of: CORO, FUNC, FUNCORO, FUNCORO32
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

# By default, download the TMC repos as CPM packages.
# If this option is enabled, CMake will expect them to be in git submodules.
# You must run `submodule update --init --recursive` before turning this on.
# This option exists to simplify my development across repositories.
option(TMC_AS_SUBMODULE "Download TMC repos as Git submodules" OFF)

if(TMC_AS_SUBMODULE)
    include_directories(
        ${runtime_benchmarks_TooManyCooks_SOURCE_DIR}/submodule/include
    )
else()
    CPMAddPackage(
        NAME TooManyCooks
        GIT_REPOSITORY https://github.com/tzcnt/TooManyCooks.git
        GIT_TAG 5fcbb9f5484dbf1aaca96af7eaba640a1be88cc0
        DOWNLOAD_ONLY)
    include_directories(
        ${TooManyCooks_SOURCE_DIR}/include
    )
endif()

# HWLOC support
message(STATUS "TMC_USE_HWLOC: running find_package(libhwloc)...")
find_package(libhwloc)

if(NOT LIBHWLOC_FOUND)
    message(FATAL_ERROR "TMC_USE_HWLOC: libhwloc was not found.")
else()
    message(STATUS "TMC_USE_HWLOC: using ${LIBHWLOC_LIBRARY}")
    add_compile_definitions(TMC_USE_HWLOC)
    link_libraries(${LIBHWLOC_LIBRARY})
endif()

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
    message(FATAL_ERROR "tcmalloc was not found.")
endif()

link_libraries(${MALLOC_LIB})

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

add_executable(matmul matmul.cpp)
