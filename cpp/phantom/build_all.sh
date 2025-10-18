cmake --preset clang-linux-release .
cmake --build ./build --parallel 16 --target fib matmul nqueens skynet
