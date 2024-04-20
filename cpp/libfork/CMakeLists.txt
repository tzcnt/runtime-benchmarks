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

# Boost (atomic) really helps speed up libforks lock free datastructures due to bad Clang codegen.
CPMAddPackage(
  NAME Boost
  VERSION 1.84.0
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_HASH SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
  OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_INCLUDE_LIBRARIES atomic"
)

# Becuase boost is not installed libfork would error if it tried to export the target.
set(CMAKE_SKIP_INSTALL_RULES On)

# My fork only fixes finding dependencies on my machine - no code changes to libfork.
# If you want, you can use the original libfork:
# https://github.com/ConorWilliams/libfork.git@v3.7.2
CPMAddPackage(
    NAME libfork
    GIT_REPOSITORY https://github.com/conorwilliams/libfork.git
    GIT_TAG v3.7.2
    DOWNLOAD_ONLY
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


add_executable(fib fib.cpp)
target_link_libraries(fib ${MALLOC_LIB} libfork::libfork)

add_executable(skynet skynet.cpp)
target_link_libraries(skynet ${MALLOC_LIB} libfork::libfork)