cmake --preset clang-macos-release .
cmake --build ./build --parallel 16 --target bench-fib matmul nqueens skynet
mv ./build/bench-fib ./build/fib
