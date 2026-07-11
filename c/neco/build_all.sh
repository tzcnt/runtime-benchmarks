# gcc is the default preset: measured on system gcc 14.3 vs clang 21.1, the
# gcc build's channel is ~37% faster while the clang build's io_socket_st is
# only ~15% faster, so gcc wins on mean ratio.
PRESET=${1:-"gcc-linux-release"}
cmake --preset $PRESET .
cmake --build ./build --parallel 16 --target all
