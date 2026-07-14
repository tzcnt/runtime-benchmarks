// 2 threads with separate event loops:
// Thread 1: A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/
// Thread 2: A client that sends a static request to the server
// and reads back data
// Thread 2 may initiate multiple parallel connections to the server

// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "memusage.hpp"

#include <boost/corosio/backend.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/ipv4_address.hpp>
#include <boost/corosio/shutdown_type.hpp>
#include <boost/corosio/tcp.hpp>
#include <boost/corosio/tcp_acceptor.hpp>
#include <boost/corosio/tcp_socket.hpp>

#include <boost/capy/buffers.hpp>
#include <boost/capy/error.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/io_task.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/when_all.hpp>
#include <boost/capy/write.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <future>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace corosio = boost::corosio;
namespace capy = boost::capy;

// Select the corosio I/O backend at compile time. Both are single-threaded here
// (one event-loop thread per io_context). The default is the epoll reactor:
// benchmarking found it ~2x faster than the io_uring proactor for this strict,
// low-concurrency request/response ping-pong (io_uring's per-operation
// submission/completion overhead does not pay off here). Define
// COROSIO_USE_IO_URING to select the io_uring proactor instead.
#if defined(COROSIO_USE_IO_URING)
#define COROSIO_BACKEND corosio::io_uring
#else
#define COROSIO_BACKEND corosio::epoll
#endif

const uint16_t PORT = 55550;
static size_t REQUEST_COUNT = 100000;
static size_t CONNECTION_COUNT = 20;

const std::string static_response = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!)";

const std::string static_request =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

// Serve requests on a single accepted connection until the peer closes it.
// The payload of the returned io_result is the number of requests served.
static capy::io_task<size_t> server_handler(corosio::tcp_socket sock) {
  char data[4096];
  for (size_t i = 0;; ++i) {
    auto [ec, n] = co_await sock.read_some(capy::mutable_buffer(data, sizeof data));
    if (ec) {
      // eof (client closed) or any other error: stop serving this connection.
      sock.close();
      co_return capy::io_result<size_t>{{}, i};
    }

    auto [wec, wn] = co_await capy::write(
      sock, capy::const_buffer(static_response.data(), static_response.size())
    );
    if (wec) {
      sock.close();
      co_return capy::io_result<size_t>{{}, i};
    }
  }
}

// Accept CONNECTION_COUNT connections, then run all handlers concurrently on
// this (single-threaded) io_context until every peer disconnects.
static capy::task<void> server(corosio::tcp_acceptor& acceptor) {
  std::vector<capy::io_task<size_t>> handlers;
  handlers.reserve(CONNECTION_COUNT);

  auto& ioc = static_cast<corosio::io_context&>(acceptor.context());
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    corosio::tcp_socket peer(ioc);
    auto [ec] = co_await acceptor.accept(peer);
    if (ec) {
      std::printf("FAIL in accept: %s\n", ec.message().c_str());
      std::terminate();
    }
    handlers.push_back(server_handler(std::move(peer)));
  }

  auto [ec, counts] = co_await capy::when_all(std::move(handlers));

  size_t total = 0;
  for (size_t i = 0; i < counts.size(); ++i) {
    total += counts[i];
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static capy::io_task<>
client_handler(corosio::io_context& ioc, uint16_t Port, size_t Count) {
  corosio::tcp_socket s(ioc);
  if (auto [ec] = co_await s.connect(
        corosio::endpoint(corosio::ipv4_address::loopback(), Port)
      );
      ec) {
    std::printf("FAIL in connect: %s\n", ec.message().c_str());
    std::terminate();
  }

  char response_buf[4096];
  for (size_t i = 0; i < Count; ++i) {
    if (auto [wec, wn] = co_await capy::write(
          s, capy::const_buffer(static_request.data(), static_request.size())
        );
        wec) {
      s.close();
      co_return capy::io_result<>{wec};
    }
    if (auto [ec, n] =
          co_await s.read_some(capy::mutable_buffer(response_buf, sizeof response_buf));
        ec) {
      s.close();
      co_return capy::io_result<>{ec};
    }
  }

  s.shutdown(corosio::shutdown_type::shutdown_both);
  s.close();
  co_return capy::io_result<>{};
}

// Open CONNECTION_COUNT connections and drive them concurrently on this
// (single-threaded) io_context, splitting REQUEST_COUNT requests across them.
static capy::task<void> client(corosio::io_context& ioc, uint16_t Port) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  std::vector<capy::io_task<>> clients;
  clients.reserve(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(client_handler(ioc, Port, count));
  }
  std::ignore = co_await capy::when_all(std::move(clients));
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }

  // Server and client each own a single-threaded io_context, each driven by its
  // own OS thread. The server binds and listens before signalling the main
  // thread, so the client never races the listen socket.
  corosio::io_context server_ioc(COROSIO_BACKEND, 1);
  std::promise<void> listening;
  std::thread server_thread([&] {
    corosio::tcp_acceptor acceptor(server_ioc);
    acceptor.open(corosio::tcp::v4());
    if (auto ec = acceptor.bind(corosio::endpoint(PORT)); ec) {
      std::printf("FAIL in bind: %s\n", ec.message().c_str());
      std::terminate();
    }
    if (auto ec = acceptor.listen(); ec) {
      std::printf("FAIL in listen: %s\n", ec.message().c_str());
      std::terminate();
    }
    listening.set_value();

    capy::run_async(server_ioc.get_executor())(server(acceptor));
    server_ioc.run();
  });

  listening.get_future().wait();

  corosio::io_context client_ioc(COROSIO_BACKEND, 1);

  auto startTime = std::chrono::high_resolution_clock::now();

  std::thread client_thread([&] {
    capy::run_async(client_ioc.get_executor())(client(client_ioc, PORT));
    client_ioc.run();
  });

  client_thread.join();
  server_thread.join();

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("connections: %zu\n", CONNECTION_COUNT);
  std::printf("runs:\n");
  std::printf("  - iteration_count: 1\n");
  std::printf("    requests: %zu\n", REQUEST_COUNT);
  std::printf("    duration: %zu us\n", totalTimeUs.count());
  std::printf(
    "    requests/sec: %zu\n", REQUEST_COUNT * 1000000 / totalTimeUs.count()
  );
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  return 0;
}
