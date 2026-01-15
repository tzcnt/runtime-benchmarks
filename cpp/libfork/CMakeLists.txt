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
)

include(../1CMake/CPM.cmake)

CPMAddPackage(
    NAME libfork
    GIT_REPOSITORY https://github.com/ConorWilliams/libfork.git
    GIT_TAG v3.8.0
)
include_directories(
    ${libfork_SOURCE_DIR}/include
    "../2common"
)


option(LF_USE_HWLOC "libhwloc" ON)

if(LF_USE_HWLOC)
    if(APPLE)
        message(STATUS "LF_USE_HWLOC: disabled on MacOS")
    else()
        message(STATUS "LF_USE_HWLOC: running find_package(libhwloc)...")
        find_package(libhwloc)

        if(NOT LIBHWLOC_FOUND)
            message(WARNING "LF_USE_HWLOC: WARNING - libhwloc was not found. Building without it... this may affect performance.")
        else()
            message(STATUS "LF_USE_HWLOC: found library ${LIBHWLOC_LIBRARY}")
            message(STATUS "LF_USE_HWLOC: found include ${LIBHWLOC_INCLUDE_DIR}/hwloc.h")
            link_libraries(${LIBHWLOC_LIBRARY})
            include_directories(${LIBHWLOC_INCLUDE_DIR})
            add_compile_definitions(LF_USE_HWLOC)
        endif()
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

link_libraries(${MALLOC_LIB} libfork::libfork)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)
