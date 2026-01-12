PRESET=${1:-"clang-linux-release"}
cmake --preset $PRESET .
cmake --build ./build --parallel 16 --target bench-fib matmul nqueens skynet
mv ./build/bench-fib ./build/fib
