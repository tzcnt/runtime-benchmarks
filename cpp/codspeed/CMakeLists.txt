cmake_minimum_required(VERSION 3.16)
project(runtime_benchmarks_codspeed)

set(CMAKE_MODULE_PATH
    ${runtime_benchmarks_codspeed_SOURCE_DIR}/../1CMake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

add_definitions(
    "-DTF_ENABLE_ATOMIC_NOTIFIER"
)

include(../1CMake/CPM.cmake)

CPMAddPackage(
    NAME taskflow
    GIT_REPOSITORY https://github.com/taskflow/taskflow.git
    GIT_TAG 1ac5852c5a1679face4a4755eaab5dbcc558128e
    DOWNLOAD_ONLY)

set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)

include(FetchContent)
FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/CodSpeedHQ/codspeed-cpp
    SOURCE_SUBDIR google_benchmark
    GIT_TAG main
)
FetchContent_MakeAvailable(google_benchmark)

include_directories(
    ${taskflow_SOURCE_DIR}
    "../2common"
)

add_executable(bench_fib bench_fib.cpp)
target_link_libraries(bench_fib benchmark::benchmark)

add_executable(bench_skynet bench_skynet.cpp)
target_link_libraries(bench_skynet benchmark::benchmark)

add_executable(bench_nqueens bench_nqueens.cpp)
target_link_libraries(bench_nqueens benchmark::benchmark)
target_compile_options(bench_nqueens PRIVATE "-falign-loops=64")

add_executable(bench_matmul bench_matmul.cpp)
target_link_libraries(bench_matmul benchmark::benchmark)
