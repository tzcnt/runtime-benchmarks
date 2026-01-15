cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_cppcoro)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_cppcoro_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-march=native"
)

include(../1CMake/CPM.cmake)

include_directories("../2common")

SET(BUILD_TESTING OFF CACHE BOOL "Build cppcoro tests" FORCE)

# using the maintained fork of cppcoro with CMake support
CPMAddPackage(
    NAME cppcoro
    GIT_REPOSITORY https://github.com/andreasbuhr/cppcoro.git
    GIT_TAG f05170fbc71aaae06dc31e32812f1caba9525377
)

include_directories(${cppcoro_SOURCE_DIR}/include)
link_libraries(cppcoro)

# Since each new coroutine requires an allocation,
# they are sensitive to allocator performance.
# Any of tcmalloc, mimalloc, or jemalloc provide
# greatly superior performance to the default glibc malloc.
# Try to find any of these 3 before falling back to default.

# NOTE: cppcoro is a Windows-aligned project and runs best with mimalloc (which is also Microsoft).
# tcmalloc, despite being the best-performing for other libraries, performs worse than the default
# allocator on my machines for some of the cppcoro benchmarks, so it has been disabled.
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
        if(APPLE)
            # tcmalloc performs well on Apple for cppcoro
            find_package(libtcmalloc)

            if(LIBTCMALLOC_FOUND)
                set(MALLOC_LIB "${LIBTCMALLOC_LIBRARY}")
                message(STATUS "Using malloc: ${MALLOC_LIB}")
            else()
                message(STATUS "Using malloc: default")
            endif()
        else()
            message(STATUS "Using malloc: default")
        endif()
    endif()
endif()

link_libraries(${MALLOC_LIB})

add_executable(fib fib.cpp)

add_executable(skynet skynet.cpp)

add_executable(nqueens nqueens.cpp)

# nqueens is particularly sensitive to misaligned loops,
# which could cause minor library changes to cause big performance variations
target_compile_options(nqueens PRIVATE "-falign-loops=64")

add_executable(matmul matmul.cpp)

add_executable(io_socket_st io_socket_st.cpp)
