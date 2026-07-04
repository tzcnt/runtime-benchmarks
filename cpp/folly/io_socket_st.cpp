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

#include <folly/SocketAddress.h>
#include <folly/coro/Collect.h>
#include <folly/coro/Task.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/io/coro/ServerSocket.h>
#include <folly/io/coro/Transport.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

const uint16_t PORT = 55550;
static size_t REQUEST_COUNT = 100000;
static size_t CONNECTION_COUNT = 20;

// Passing a timeout of 0 to Transport::read()/write() means no timeout.
static constexpr std::chrono::milliseconds no_timeout(0);

const std::string static_response = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!)";

const std::string static_request =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

static folly::ByteRange as_bytes(const std::string& S) {
  return folly::ByteRange(
    reinterpret_cast<const unsigned char*>(S.data()), S.size()
  );
}

// Transport's virtual read() overrides hide the inherited
// read(void*, size_t, timeout) convenience overload, so build the
// MutableByteRange explicitly.
static folly::MutableByteRange as_mutable_bytes(char* Buf, size_t Len) {
  return folly::MutableByteRange(reinterpret_cast<unsigned char*>(Buf), Len);
}

struct result {
  bool error;
  size_t recv_count;
};

folly::coro::Task<result>
server_handler(std::unique_ptr<folly::coro::Transport> Socket) {
  char data[4096];
  for (size_t i = 0;; ++i) {
    try {
      size_t n =
        co_await Socket->read(as_mutable_bytes(data, sizeof(data)), no_timeout);
      if (n == 0) {
        // EOF (client disconnected first)
        Socket->close();
        co_return result{false, i};
      }
      co_await Socket->write(as_bytes(static_response));
    } catch (const std::exception&) {
      Socket->close();
      co_return result{true, i};
    }
  }
}

static folly::coro::Task<void> server(folly::EventBase* Evb, uint16_t Port) {
  folly::coro::ServerSocket acceptor(
    folly::AsyncServerSocket::newSocket(Evb),
    folly::SocketAddress("127.0.0.1", Port), /*listenQueueDepth=*/1024
  );

  // Wait for CONNECTION_COUNT connections to be opened. The client opens all
  // of its connections up front, so the handlers can be started as one batch
  // afterward; requests sent in the meantime just wait in the socket buffers.
  std::vector<folly::coro::Task<result>> handlers;
  handlers.reserve(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    std::unique_ptr<folly::coro::Transport> sock;
    try {
      sock = co_await acceptor.accept();
    } catch (const std::exception& e) {
      std::printf("FAIL in accept: %s", e.what());
      std::terminate();
    }
    handlers.push_back(server_handler(std::move(sock)));
  }
  // Wait for all handlers to complete and then count the results
  auto results = co_await folly::coro::collectAllRange(std::move(handlers));

  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto& result = results[i];
    // Expect success (completed all requests); EOF (client disconnected
    // first) is not an error
    if (result.error) {
      std::printf("FAIL in server\n");
    }
    total += result.recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static folly::coro::Task<void>
client_handler(folly::EventBase* Evb, uint16_t Port, size_t Count) {
  auto s = co_await folly::coro::Transport::newConnectedSocket(
    Evb, folly::SocketAddress("127.0.0.1", Port),
    std::chrono::milliseconds(1000)
  );

  char response_buf[4096];
  size_t i = 0;
  for (; i < Count; ++i) {
    try {
      co_await s.write(as_bytes(static_request));
      size_t n = co_await s.read(
        as_mutable_bytes(response_buf, sizeof(response_buf)), no_timeout
      );
      if (n == 0) {
        break;
      }
    } catch (const std::exception&) {
      break;
    }
  }
  if (i != Count) {
    std::printf("FAIL in client: finished early\n");
    std::terminate();
  }

  s.shutdownWrite();
  s.close();
}

static folly::coro::Task<void> client(folly::EventBase* Evb, uint16_t Port) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  std::vector<folly::coro::Task<void>> clients;
  clients.reserve(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(client_handler(Evb, Port, count));
  }
  co_await folly::coro::collectAllRange(std::move(clients));
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }

  folly::ScopedEventBaseThread server_thread;
  folly::ScopedEventBaseThread client_thread;

  auto server_future =
    co_withExecutor(
      server_thread.getEventBase(), server(server_thread.getEventBase(), PORT)
    )
      .start();
  // Ensure that the socket is actually open before sending traffic,
  // or the client will fail immediately.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto startTime = std::chrono::high_resolution_clock::now();
  auto client_future =
    co_withExecutor(
      client_thread.getEventBase(), client(client_thread.getEventBase(), PORT)
    )
      .start();
  std::move(client_future).get();
  std::move(server_future).get();
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("connections: %zu\n", CONNECTION_COUNT);
  std::printf("runs:\n");
  std::printf("  - iteration_count: 1\n");
  std::printf("    requests: %zu\n", REQUEST_COUNT);
  std::printf("    duration: %zu us\n", static_cast<size_t>(totalTimeUs.count()));
  std::printf(
    "    requests/sec: %zu\n",
    REQUEST_COUNT * 1000000 / static_cast<size_t>(totalTimeUs.count())
  );
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
}
