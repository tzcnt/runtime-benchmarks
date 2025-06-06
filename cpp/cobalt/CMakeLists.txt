cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_cobalt)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_cobalt_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

# Tell cmake to use the target Boost cmake file instead of the built-in
cmake_policy(SET CMP0167 NEW)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_RELEASE_LIBS ON)

# set(ENV{BOOST_CMAKE_ROOT} "/home/tzcnt/pkg-src/boost-1.88-src/boost_1_88_0/stage/lib/cmake/")

# By default this will will look for your system boost.
# However, on my distro the system boost does not include boost::cobalt, so I'm using a local build.
# I've set the system environment variable BOOST_CMAKE_ROOT=/home/tzcnt/pkg-src/boost-1.88-src/boost_1_88_0/stage/lib/cmake/
# with the /stage/lib/cmake directory being where boost places the CMake files after building with b2.
if(DEFINED ENV{BOOST_CMAKE_ROOT})
    find_package(
        Boost 1.82.0 REQUIRED CONFIG
        PATHS $ENV{BOOST_CMAKE_ROOT} NO_DEFAULT_PATH
        COMPONENTS cobalt
    )
else()
    find_package(Boost 1.82.0 REQUIRED COMPONENTS cobalt)
endif()

link_libraries(Boost::cobalt)

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

add_executable(channel channel.cpp)

add_executable(io_socket_st io_socket_st.cpp)
