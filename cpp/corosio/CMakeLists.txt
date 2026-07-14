cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_corosio)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_corosio_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

# Corosio (the async networking library) is a combo: it requires Capy, a
# coroutine-foundation library that supplies the task type, the thread_pool
# executor, and the when_all fork-join combinator. Corosio itself supplies the
# io_context, tcp_socket, and tcp_acceptor used by io_socket_st.
#
# Both are standalone Boost-candidate libraries (they depend only on Threads and
# an optional liburing; corosio additionally consumes the Boost::capy target
# provided by the consumer). Neither depends on a full Boost distribution.
#
# The library-ref override applies to corosio (the headline library); Capy is
# pinned to a commit that is API-compatible with the pinned corosio.
set(COROSIO_GIT_TAG 4276bd4039097fd4dd62dda4bb55e53d03351088)
if(DEFINED ENV{RUNTIME_BENCHMARKS_LIBRARY_REF})
    set(COROSIO_GIT_TAG "$ENV{RUNTIME_BENCHMARKS_LIBRARY_REF}")
endif()
set(CAPY_GIT_TAG 9144290189fa149b27617c7d9a476c8fbffb7b8c)

# Make corosio's optional io_uring backend build correctly when liburing is
# present. corosio's bundled Findliburing.cmake creates the liburing::liburing
# imported target only inside its own add_subdirectory scope, and its
# IMPORTED_GLOBAL promotion does not survive out to the consumer under CPM,
# leaving boost_corosio with an unresolved liburing::liburing reference at link
# time (worse: that module reports liburing_FOUND=TRUE even when it fails to
# create the target, so corosio enables io_uring anyway and the link breaks).
#
# Fix: try to create the liburing::liburing target ourselves, at top-level scope
# and GLOBAL, from pkg-config. If that succeeds, corosio finds the existing
# target (`if(NOT TARGET liburing::liburing)`) and links it. If liburing or
# pkg-config is unavailable, we disable corosio's own liburing detection so it
# builds epoll-only instead of referencing a target that was never created.
#
# io_uring is NOT the default backend: benchmarking io_socket_st showed the
# io_uring proactor runs ~2x slower than epoll for this single-threaded, strict
# request/response ping-pong (per-op submission/completion overhead dominates at
# low concurrency). epoll is the default (see io_socket_st.cpp); when liburing is
# available the io_uring backend is compiled in and can be selected with
# -DCOROSIO_USE_IO_URING.
set(COROSIO_HAVE_LIBURING OFF)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBURING QUIET IMPORTED_TARGET GLOBAL liburing>=2.5)
    if(LIBURING_FOUND)
        if(NOT TARGET liburing::liburing)
            add_library(liburing::liburing ALIAS PkgConfig::LIBURING)
        endif()
        set(COROSIO_HAVE_LIBURING ON)
    endif()
endif()
if(NOT COROSIO_HAVE_LIBURING)
    set(CMAKE_DISABLE_FIND_PACKAGE_liburing ON CACHE BOOL "" FORCE)
endif()

# Silence each library's own tests / examples / benchmarks / perf tools so only
# the library targets are configured. These are honored via CMP0077 (a normal
# variable overrides the project's option()).
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BOOST_CAPY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BOOST_CAPY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BOOST_CAPY_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(BOOST_COROSIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BOOST_COROSIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BOOST_COROSIO_BUILD_PERF OFF CACHE BOOL "" FORCE)

# Capy must be added before Corosio so the Boost::capy target exists when
# Corosio references it.
CPMAddPackage(
    NAME capy
    GIT_REPOSITORY https://github.com/cppalliance/capy.git
    GIT_TAG ${CAPY_GIT_TAG})

CPMAddPackage(
    NAME corosio
    GIT_REPOSITORY https://github.com/cppalliance/corosio.git
    GIT_TAG ${COROSIO_GIT_TAG})

include_directories("../2common")

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

link_libraries(${MALLOC_LIB} Boost::corosio Boost::capy)

# Fork-join benchmarks: capy::thread_pool + capy::when_all
add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

# Networking benchmark: corosio::io_context + tcp_acceptor/tcp_socket
add_executable(io_socket_st io_socket_st.cpp)
