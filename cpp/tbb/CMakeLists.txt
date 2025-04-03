cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_TBB)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_TBB_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

# this requires a system-installed version of Intel OneAPI / TBB
# On Debian the package is "intel-oneapi-tbb-devel"
# include("/opt/intel/oneapi/tbb/latest/lib/cmake/tbb/TBBConfig.cmake")
set(TBB_DIR "/opt/intel/oneapi/tbb/latest/lib/cmake/tbb/")

# list(APPEND CMAKE_MODULE_PATH "/opt/intel/oneapi/tbb/latest/lib/cmake/tbb/")
find_package(TBB REQUIRED)
include_directories(
    ${TBB_SOURCE_DIR}/include
)
link_libraries(TBB::tbb)

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

add_executable(matmul matmul.cpp)
