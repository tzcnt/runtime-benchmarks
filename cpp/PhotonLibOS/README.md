PhotonLibOS is a fiber runtime by Alibaba.

A vendored copy of Release v0.8.2 is included here, which fixes this bug:
https://github.com/alibaba/PhotonLibOS/pull/749

Otherwise the build fails on this warning, even when building with clang:
runtime-benchmarks/cpp/PhotonLibOS/deps/PhotonLibOS/net/http/server.cpp:304:22
warning: variable length arrays in C++ are a Clang extension [-Wvla-cxx-extension]