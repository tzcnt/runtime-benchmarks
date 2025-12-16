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

add_compile_options("-mcx16")

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
        GIT_TAG v1.1.0
        DOWNLOAD_ONLY)
    include_directories(
        ${TooManyCooks_SOURCE_DIR}/include
    )
endif()

# HWLOC support
option(TMC_USE_HWLOC "libhwloc" ON)

if(TMC_USE_HWLOC)
    message(STATUS "TMC_USE_HWLOC: running find_package(libhwloc)...")
    find_package(libhwloc)

    if(NOT LIBHWLOC_FOUND)
        message(WARNING "TMC_USE_HWLOC: WARNING - libhwloc was not found. Building without it... this may affect performance.")
    else()
        message(STATUS "TMC_USE_HWLOC: using ${LIBHWLOC_LIBRARY}")
        add_compile_definitions(TMC_USE_HWLOC)
        link_libraries(${LIBHWLOC_LIBRARY})
    endif()
endif()

# Get snmalloc per https://github.com/microsoft/snmalloc/blob/main/docs/BUILDING.md
set(SNMALLOC_HEADER_ONLY_LIBRARY ON)
CPMAddPackage(
    NAME snmalloc
    GIT_REPOSITORY https://github.com/microsoft/snmalloc.git
    GIT_TAG 0.7.3
    DOWNLOAD_ONLY)

include_directories(
    ${snmalloc_SOURCE_DIR}/src
)

add_compile_definitions(SNMALLOC_USE_WAIT_ON_ADDRESS=0)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

add_executable(matmul matmul.cpp)
