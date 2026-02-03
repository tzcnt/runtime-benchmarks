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
        GIT_TAG v1.4
        DOWNLOAD_ONLY)

    include_directories(
        ${TooManyCooks_SOURCE_DIR}/include
    )
endif()

option(TMC_USE_BOOST_ASIO "Use boost::asio instead of standalone asio" OFF)

if(TMC_USE_BOOST_ASIO)
    add_compile_definitions(TMC_USE_BOOST_ASIO)

    # Tell cmake to use the target Boost cmake file instead of the built-in
    cmake_policy(SET CMP0167 NEW)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_RELEASE_LIBS ON)

    # set(ENV{BOOST_CMAKE_ROOT} "/home/tzcnt/pkg-src/boost-1.88-src/boost_1_88_0/stage/lib/cmake/")

    # By default this will will look for your system boost.
    # Or you can set BOOST_CMAKE_ROOT to tell it where to look.
    if(DEFINED ENV{BOOST_CMAKE_ROOT})
        find_package(
            Boost 1.82.0 REQUIRED CONFIG
            PATHS $ENV{BOOST_CMAKE_ROOT} NO_DEFAULT_PATH
        )
    else()
        find_package(Boost 1.82.0 REQUIRED)
    endif()
else()
    CPMAddPackage(
        NAME asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        VERSION 1.36.1
        GIT_TAG asio-1-36-0
        DOWNLOAD_ONLY)

    # mark Asio headers as SYSTEM to ignore warnings
    include_directories(SYSTEM
        ${asio_SOURCE_DIR}/asio/include
    )
endif()

CPMAddPackage(
    NAME tmc_asio
    GIT_REPOSITORY https://github.com/tzcnt/tmc-asio.git
    GIT_TAG v1.4
    DOWNLOAD_ONLY)
include_directories(
    ${tmc_asio_SOURCE_DIR}/include
)

# HWLOC support
option(TMC_USE_HWLOC "libhwloc" ON)

if(TMC_USE_HWLOC)
    message(STATUS "TMC_USE_HWLOC: running find_package(libhwloc)...")
    find_package(libhwloc)

    if(NOT LIBHWLOC_FOUND)
        message(WARNING "TMC_USE_HWLOC: WARNING - libhwloc was not found. Building without it... this may affect performance.")
    else()
        message(STATUS "TMC_USE_HWLOC: found library ${LIBHWLOC_LIBRARY}")
        message(STATUS "TMC_USE_HWLOC: found include ${LIBHWLOC_INCLUDE_DIR}/hwloc.h")
        link_libraries(${LIBHWLOC_LIBRARY})
        include_directories(${LIBHWLOC_INCLUDE_DIR})
        add_compile_definitions(TMC_USE_HWLOC)
    endif()
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

link_libraries(${MALLOC_LIB})

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

add_executable(threads_sweep threads_sweep.cpp)

add_executable(channel channel.cpp)

add_executable(io_socket_st io_socket_st.cpp)
