cmake --preset clang-macos-release .
cmake --build ./build --parallel 16 --target fib matmul nqueens skynet
