cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_HPX)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_HPX_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(HPX_WITH_CXX_STANDARD 20 CACHE STRING "")

# This config is very important - enabling mmap causes 10x slowdown
# because of IPI storm when releasing the pages and calling madvise(MADV_DONTNEED)
set(HPX_WITH_THREAD_STACK_MMAP OFF CACHE BOOL "")

# Enable trying different schedulers - although they made minimal difference
set(HPX_THREAD_SCHEDULERS "all" CACHE STRING "")

# Remove some unnecessary / possibly performance sapping features
set(HPX_WITH_STACKTRACES OFF CACHE BOOL "")
set(HPX_WITH_THREAD_CUMULATIVE_COUNTS OFF CACHE BOOL "")
set(HPX_WITH_THREAD_GUARD_PAGE OFF CACHE BOOL "")

# Didn't make a difference
# set(HPX_WITH_STATIC_LINKING "ON" CACHE BOOL "")

# This one makes things much slower if enabled, but also limits memory usage
# I suspect it is preventing forking / forcing serial execution
# set(HPX_COROUTINES_WITH_THREAD_SCHEDULE_HINT_RUNS_AS_CHILD ON CACHE BOOL "")

# HPX requires Asio but we don't want to require system Asio
set(HPX_WITH_FETCH_ASIO ON CACHE BOOL "")

# Remove unnecessary builds
set(HPX_WITH_EXAMPLES OFF CACHE BOOL "")
set(HPX_WITH_TESTS OFF CACHE BOOL "")
set(HPX_WITH_NETWORKING OFF CACHE BOOL "")
set(HPX_WITH_DISTRIBUTED_RUNTIME OFF CACHE BOOL "")

# This CMakeLists file will find and link the correct malloc; we don't need HPX to do it.
# (or else conflicting libraries may be linked / same library may be linked twice)
set(HPX_WITH_MALLOC "default" CACHE STRING "")

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

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)
