cmake_minimum_required(VERSION 3.24)
project(runtime_benchmarks_folly)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_folly_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(

    # Performance tuning options
    "-march=native"
)

include(../1CMake/CPM.cmake)

# folly locates fmt via find_package(fmt CONFIG REQUIRED). If it isn't
# installed on the system, fetch it and let CMake redirect folly's
# find_package call to the fetched copy (OVERRIDE_FIND_PACKAGE needs
# CMake 3.24+, hence the cmake_minimum_required above).
find_package(fmt CONFIG QUIET)

if(NOT fmt_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.2.0
        OVERRIDE_FIND_PACKAGE)
    FetchContent_MakeAvailable(fmt)
endif()

# folly's other required dependencies (Boost, glog, gflags, double-conversion,
# libevent, OpenSSL, fast_float, zlib) are expected to be system-installed.

# v2025.11.17.00 is the newest tag whose io_uring code still compiles against
# released liburing headers. folly commits after 2025-11-20 track unreleased
# liburing uapi (io_uring zero-copy receive) and fail to build against the
# liburing that distros ship (2.14 as of this writing).
set(FOLLY_GIT_TAG v2025.11.17.00)

if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(FOLLY_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()

CPMAddPackage(
    NAME folly
    GIT_REPOSITORY https://github.com/facebook/folly.git
    GIT_TAG ${FOLLY_GIT_TAG}
    OPTIONS
    "BUILD_TESTS OFF"
    "BUILD_BENCHMARKS OFF")

include_directories("../2common")

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

link_libraries(${MALLOC_LIB} folly)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

# # The Unbalanced Tree Search benchmark needs the UTS tree-generation code,
# # which is plain C (uts.c) plus the SHA1-based RNG (brg_sha1.c). These live in
# # 2common (already on the include path) and are shared across runtimes.
# add_executable(uts
# uts.cpp
# ../2common/uts/external/uts.c
# ../2common/uts/external/rng/brg_sha1.c)
add_executable(channel channel.cpp)

add_executable(io_socket_st io_socket_st.cpp)
