cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_libcoro)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_libcoro_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

# SET(GIT_SUBMODULES_RECURSE ON CACHE BOOL "Download git submodules" FORCE)
include(../1CMake/CPM.cmake)

include_directories("../2common")

SET(LIBCORO_BUILD_TESTS OFF CACHE BOOL "Build libcoro tests" FORCE)
SET(LIBCORO_BUILD_EXAMPLES OFF CACHE BOOL "Build libcoro examples" FORCE)
SET(LIBCORO_FEATURE_NETWORKING OFF CACHE BOOL "libcoro networking" FORCE)
SET(LIBCORO_FEATURE_TLS OFF CACHE BOOL "libcoro TLS (networking)" FORCE)

CPMAddPackage(
    NAME libcoro
    GIT_REPOSITORY https://github.com/jbaldwin/libcoro.git
    GIT_TAG v0.15.0
    DOWNLOAD_ONLY)

include_directories(${libcoro_SOURCE_DIR}/include)
link_libraries(libcoro)

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
