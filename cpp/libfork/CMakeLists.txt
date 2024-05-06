cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_libfork)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_libfork_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    # Performance tuning options
    "-march=native"
    # Instructs libfork to use hwloc.
    "-DLF_USE_HWLOC"
)

include(../1CMake/CPM.cmake)

# Libfork requires Boost for optimal performance. It will try to find Boost on its own.
# If you don't have Boost installed, it will still run (you can comment out the next line),
# but performance may be degraded substantially.
find_package(Boost 1.83.0 REQUIRED COMPONENTS atomic)

# My fork only fixes finding dependencies on my machine - no code changes to libfork.
# If you want, you can use the original libfork:
# https://github.com/ConorWilliams/libfork.git@v3.7.2
CPMAddPackage(
    NAME libfork
    GIT_REPOSITORY https://github.com/tzcnt/libfork.git
    GIT_TAG 3655643a2b612817c16fc9abf99e6c8377cfb3bc
    DOWNLOAD_ONLY)
include_directories(
    ${libfork_SOURCE_DIR}/include
)

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

link_libraries(${MALLOC_LIB} libfork::libfork hwloc)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)
