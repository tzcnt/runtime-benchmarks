cmake_minimum_required(VERSION 3.20)
project(runtime_benchmarks_PhotonLibOS)

# ./cmake (a Finduring.cmake that accepts modern liburing) must precede
# Photon's own CMake/ dir, which it appends to CMAKE_MODULE_PATH.
set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_PhotonLibOS_SOURCE_DIR}/cmake
    ${runtime_benchmarks_PhotonLibOS_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(

    # Performance tuning options
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

set(PHOTON_GIT_TAG v0.9.4)

if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(PHOTON_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()

# Photon v0.9.4's common/stream.h uses uint64_t without including <cstdint>,
# which no longer arrives transitively with current libstdc++ (fixed upstream
# after v0.9.4). Force-include it rather than patching the fetched source; it
# is a no-op on refs that include it themselves.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include cstdint")

# Photon builds itself with an unconditional -Werror, which newer clang's
# added warnings (e.g. -Winconsistent-missing-override in its fs/ code) turn
# into build failures. Directory compile options are emitted after
# CMAKE_CXX_FLAGS on the compile line, so this -Wno-error wins.
add_compile_options(-Wno-error)

# Photon finds its required dependencies (OpenSSL, zlib, libaio, liburing) on
# the system. io_uring support is compiled in so the io_socket_st benchmark
# runs on Photon's preferred Linux event engine (INIT_EVENT_DEFAULT tries
# io_uring first, then epoll). libcurl is only needed for Photon's HTTP
# client, which the benchmarks don't use.
CPMAddPackage(
    NAME PhotonLibOS
    GIT_REPOSITORY https://github.com/alibaba/PhotonLibOS.git
    GIT_TAG ${PHOTON_GIT_TAG}
    OPTIONS
    "PHOTON_BUILD_TESTING OFF"
    "PHOTON_ENABLE_URING ON"
    "PHOTON_ENABLE_LIBCURL OFF"

    # Photon defaults to C++14, but v0.9.4's headers use std::string_view
    "PHOTON_CXX_STANDARD 17")

# Photon's fs/cache code pulls a std::string out of va_arg, which clang
# rejects by default (-Wnon-pod-varargs). The benchmarks never touch that
# code path; suppress it for Photon's own objects only.
target_compile_options(photon_obj PRIVATE -Wno-non-pod-varargs)

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

link_libraries(${MALLOC_LIB} photon_static)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

add_executable(channel channel.cpp)

add_executable(io_socket_st io_socket_st.cpp)
