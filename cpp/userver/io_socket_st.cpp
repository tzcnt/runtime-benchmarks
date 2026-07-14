// TCP ping-pong between a client and a server, both running as engine tasks
// on a shared 2-worker TaskProcessor (userver parks the calling coroutine in
// engine::io::Socket calls and polls fds on the engine's dedicated ev
// thread).
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/io_socket_st.cpp
//
// Server: accepts connection_count connections and, per connection, loops
// reading a request and writing a fixed HTTP response until the client
// disconnects.
//
// Client: opens connection_count connections and sends request_count requests
// total (split across connections), reading the response each time.
//
// The argument convention matches the C++ version: argv[1] = connection_count
// (build_and_bench_all.py fills this with the thread count), argv[2] =
// request_count. userver's Socket::Bind sets SO_REUSEADDR (like asio's
// acceptor), so back-to-back runs in the thread sweep don't hit EADDRINUSE.
// The listener is bound synchronously before the client tasks start, so there
// is no startup race.

#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/engine/async.hpp>
#include <userver/engine/io/exception.hpp>
#include <userver/engine/io/sockaddr.hpp>
#include <userver/engine/io/socket.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace engine = userver::engine;
namespace io = userver::engine::io;

static constexpr std::uint16_t kPort = 55550;
static std::size_t request_count = 100000;
static std::size_t connection_count = 20;

static const std::string static_response = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!)";

static const std::string static_request =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

static std::size_t server_handler(io::Socket socket) {
  char data[4096];
  std::size_t i = 0;
  try {
    for (;;) {
      const std::size_t n = socket.RecvSome(data, sizeof(data), {});
      if (n == 0) {
        break; // client disconnected
      }
      (void)socket.SendAll(
        static_response.data(), static_response.size(), {}
      );
      ++i;
    }
  } catch (const io::IoException&) {
    // connection error; the served-request total will catch undercounts
  }
  return i;
}

static void server(io::Socket listener) {
  std::vector<engine::TaskWithResult<std::size_t>> handlers;
  handlers.reserve(connection_count);
  for (std::size_t i = 0; i < connection_count; ++i) {
    io::Socket connection = listener.Accept({});
    handlers.push_back(
      engine::AsyncNoTracing(server_handler, std::move(connection))
    );
  }

  std::size_t total = 0;
  for (auto& handler : handlers) {
    total += handler.Get();
  }
  if (total != request_count) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", request_count, total
    );
  }
}

static void client_handler(io::Sockaddr addr, std::size_t count) {
  io::Socket s{addr.Domain(), io::SocketType::kStream};
  s.Connect(addr, {});

  char response_buf[4096];
  std::size_t done = 0;
  try {
    for (std::size_t k = 0; k < count; ++k) {
      (void)s.SendAll(static_request.data(), static_request.size(), {});
      const std::size_t n = s.RecvSome(response_buf, sizeof(response_buf), {});
      if (n == 0) {
        break;
      }
      ++done;
    }
  } catch (const io::IoException&) {
  }
  if (done != count) {
    std::printf("FAIL in client: finished early\n");
    std::abort();
  }
}

static void client(io::Sockaddr addr) {
  const std::size_t per_task = request_count / connection_count;
  const std::size_t rem = request_count % connection_count;
  std::vector<engine::TaskWithResult<void>> clients;
  clients.reserve(connection_count);
  for (std::size_t i = 0; i < connection_count; ++i) {
    const std::size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(engine::AsyncNoTracing(client_handler, addr, count));
  }
  for (auto& c : clients) {
    c.Get();
  }
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    connection_count = static_cast<std::size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    request_count = static_cast<std::size_t>(atoi(argv[2]));
  }

  // The reference runtimes run a single-threaded event loop for the server
  // and another for the client (two OS threads total); give the shared
  // TaskProcessor the same 2-thread budget, independent of connection count.
  engine::RunStandalone(2, bench::pools_config(false), [] {
    auto addr = io::Sockaddr::MakeIPv4LoopbackAddress();
    addr.SetPort(kPort);

    io::Socket listener{addr.Domain(), io::SocketType::kStream};
    listener.Bind(addr);
    listener.Listen();

    auto server_task = engine::AsyncNoTracing(server, std::move(listener));

    auto startTime = std::chrono::high_resolution_clock::now();
    client(addr);
    server_task.Get();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );

    std::printf("connections: %zu\n", connection_count);
    std::printf("runs:\n");
    std::printf("  - iteration_count: 1\n");
    std::printf("    requests: %zu\n", request_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs.count()));
    std::printf(
      "    requests/sec: %" PRIu64 "\n",
      static_cast<std::uint64_t>(request_count) * 1000000 /
        static_cast<std::uint64_t>(totalTimeUs.count())
    );
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
