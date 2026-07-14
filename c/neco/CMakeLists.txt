cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_neco C)

# Reuse the shared CMake helpers (CPM + Find modules) from the C++ suite.
set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_neco_SOURCE_DIR}/../../cpp/1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_C_STANDARD 11)

add_definitions(
    "-march=native"
)

include(../../cpp/1CMake/CPM.cmake)

set(NECO_GIT_TAG 84ed4c4f8bc7d5ea471327e63a0e37272cc9b7cb)
if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(NECO_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()

CPMAddPackage(
    NAME neco
    GIT_REPOSITORY https://github.com/tidwall/neco.git
    GIT_TAG ${NECO_GIT_TAG}
    DOWNLOAD_ONLY)

# neco ships as a single neco.c/neco.h amalgamation; build it once and link
# it into every benchmark.
add_library(neco_lib STATIC ${neco_SOURCE_DIR}/neco.c)
target_include_directories(neco_lib PUBLIC ${neco_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(neco_lib PUBLIC Threads::Threads)

link_libraries(neco_lib)

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

# Only the concurrency benchmarks are implemented: neco's scheduler is
# single-threaded (one runtime per OS thread, no work stealing), so the
# fork-join compute benchmarks (fib, skynet, nqueens, matmul) would not be a
# meaningful comparison against the multithreaded runtimes.
add_executable(channel channel.c)

add_executable(io_socket_st io_socket_st.c)
