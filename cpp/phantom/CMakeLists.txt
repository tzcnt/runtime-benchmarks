cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_phantom)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_phantom_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 23)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

CPMAddPackage(
    NAME phantom
    GIT_REPOSITORY https://github.com/JoshuaRowePhantom/Phantom.Coroutines.git
    GIT_TAG 5fc9e9941521e93265694edcdf7d1deb96642f59
    DOWNLOAD_ONLY)

include_directories(
    "${phantom_SOURCE_DIR}/Phantom.Coroutines/include"
    "../2common"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++ -latomic)
    set(ENABLE_LINK_MODULES OFF)
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

link_libraries(${MALLOC_LIB})

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

add_executable(matmul matmul.cpp)
