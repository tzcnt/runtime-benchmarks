cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_concurrencpp)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_concurrencpp_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(

    # Performance tuning options
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

option(CCPP_AS_SUBMODULE "Download concurrencpp as Git submodules" OFF)

if(CCPP_AS_SUBMODULE)
    add_subdirectory(submodule)

# include_directories(
# ${runtime_benchmarks_concurrencpp_SOURCE_DIR}/submodule/include
# )
else()
    CPMAddPackage(
        NAME concurrencpp
        GIT_REPOSITORY https://github.com/tzcnt/concurrencpp.git
        GIT_TAG 06881fdbe8eedd4831c83a412306d04b5e8aa457
        DOWNLOAD_ONLY)
    include_directories(
        ${concurrencpp_SOURCE_DIR}/include
    )
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

link_libraries(${MALLOC_LIB} concurrencpp::concurrencpp)

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

add_executable(matmul matmul.cpp)
