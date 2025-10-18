cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_HPX)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_HPX_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(HPX_WITH_CXX_STANDARD 20 CACHE STRING "")
set(HPX_WITH_FETCH_ASIO ON CACHE BOOL "")

# Remove unnecessary builds
set(HPX_WITH_EXAMPLES OFF CACHE BOOL "")
set(HPX_WITH_TESTS OFF CACHE BOOL "")
set(HPX_WITH_NETWORKING OFF CACHE BOOL "")
set(HPX_WITH_DISTRIBUTED_RUNTIME OFF CACHE BOOL "")

# On my Debian machine with Clang 21 and GCC 14 the build fails if this config is enabled
set(HPX_WITH_CXX11_ATOMIC_RESULT OFF CACHE BOOL "")
set(HPX_WITH_CXX11_ATOMIC_128BIT OFF CACHE BOOL "")

add_definitions(

    # Performance tuning options
    "-march=native"
)

include(../1CMake/CPM.cmake)

CPMAddPackage(
    NAME hpx
    GIT_REPOSITORY https://github.com/STEllAR-GROUP/hpx.git
    GIT_TAG v1.11.0)
include_directories(
    ${hpx_SOURCE_DIR}/include
    "../2common"
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

link_libraries(${MALLOC_LIB} HPX::hpx)

# Benchmark executables will be added here:
add_executable(fib fib.cpp)
add_executable(skynet skynet.cpp)
add_executable(nqueens nqueens.cpp)
add_executable(matmul matmul.cpp)
