cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_TooManyCooks)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_TooManyCooks_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

# TMC (v1.5.0+) ships a header-only `TooManyCooks::TooManyCooks` INTERFACE target. Docs: https://www.fleetcode.com/oss/tmc/docs/dev/build_flags.html

# Fixed single priority level
set(TMC_PRIORITY_COUNT 1 CACHE STRING "" FORCE)

# Topology-aware thread placement. The target finds and links hwloc itself and
# hard-errors if it is missing; pass -DTMC_USE_HWLOC=OFF to build without it.
set(TMC_USE_HWLOC ON CACHE BOOL "" FORCE)

# Trivial-task optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(TMC_TRIVIAL_TASK ON CACHE BOOL "" FORCE)
endif()

# Standalone asio by default; -DTMC_USE_BOOST_ASIO=ON switches both TMC (this
# option feeds the interface target) and the benchmark's asio headers (below) to
# boost::asio.
option(TMC_USE_BOOST_ASIO "Use boost::asio instead of standalone asio" OFF)

# By default, download the TMC repo as a CPM package.
# If TMC_AS_SUBMODULE is enabled, CMake will expect it in a git submodule instead.
# You must run `submodule update --init --recursive` before turning this on.
# This option exists to simplify my development across repositories.
option(TMC_AS_SUBMODULE "Use TMC from a Git submodule instead of CPM" OFF)

if(TMC_AS_SUBMODULE)
    add_subdirectory(
        ${runtime_benchmarks_TooManyCooks_SOURCE_DIR}/submodule
        ${CMAKE_BINARY_DIR}/tmc-submodule
    )
else()
    set(TMC_GIT_TAG main)

    if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
        set(TMC_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
    endif()

    CPMAddPackage(
        NAME TooManyCooks
        GIT_REPOSITORY https://github.com/tzcnt/TooManyCooks.git
        GIT_TAG ${TMC_GIT_TAG})
endif()

# The INTERFACE target carries TMC's include dirs, compile definitions (from the
# options above), hwloc, and pthreads.
link_libraries(TooManyCooks::TooManyCooks)

# TMC consumes Asio but does not provide it
if(TMC_USE_BOOST_ASIO)
    # Tell cmake to use the target Boost cmake file instead of the built-in
    cmake_policy(SET CMP0167 NEW)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_RELEASE_LIBS ON)

    # set(ENV{BOOST_CMAKE_ROOT} "/home/tzcnt/pkg-src/boost-1.88-src/boost_1_88_0/stage/lib/cmake/")

    # By default this will look for your system boost.
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
        DOWNLOAD_ONLY YES)

    # mark Asio headers as SYSTEM to ignore warnings
    include_directories(SYSTEM
        ${asio_SOURCE_DIR}/asio/include
    )
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
