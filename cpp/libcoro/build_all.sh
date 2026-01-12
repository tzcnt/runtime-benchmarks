PRESET=${1:-"clang-linux-release"}
cmake --preset $PRESET .
cmake --build ./build --parallel 16 --target fib matmul nqueens skynet
