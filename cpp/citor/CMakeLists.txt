cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_citor)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_citor_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

set(CITOR_GIT_TAG v0.4.5)
if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(CITOR_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()

CPMAddPackage(
    NAME citor
    GIT_REPOSITORY https://github.com/Lallapallooza/citor.git
    GIT_TAG ${CITOR_GIT_TAG}
    OPTIONS
        "CITOR_BUILD_TESTS OFF"
        "CITOR_BUILD_BENCHMARK OFF"
)

include_directories("../2common")

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

link_libraries(${MALLOC_LIB} citor::citor)

# Larger worker stack for deep recursive fork-join (fib, nqueens).
add_compile_definitions(CITOR_WORKER_STACK_KIB=65536)

add_executable(fib fib.cpp)
add_executable(skynet skynet.cpp)
add_executable(nqueens nqueens.cpp)
target_compile_options(nqueens PRIVATE "-falign-loops=64")
add_executable(matmul matmul.cpp)

# The Unbalanced Tree Search benchmark needs the UTS tree-generation code,
# which is plain C (uts.c) plus the SHA1-based RNG (brg_sha1.c). These live in
# 2common (already on the include path) and are shared across runtimes.
add_executable(uts
    uts.cpp
    ../2common/uts/external/uts.c
    ../2common/uts/external/rng/brg_sha1.c)

add_executable(uts_opt1
    uts_opt1.cpp
    ../2common/uts/external/uts.c
    ../2common/uts/external/rng/brg_sha1.c)

add_executable(uts_opt2
    uts_opt2.cpp
    ../2common/uts/external/uts.c
    ../2common/uts/external/rng/brg_sha1.c)
