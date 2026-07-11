// TCP ping-pong between a client and a server, each running a single-vcpu
// photon event loop on its own OS thread (2 OS threads total, matching the
// canonical implementation's pair of single-threaded executors). Photon
// parks the calling photon thread in socket calls and polls fds on the
// vcpu's event engine (io_uring or epoll, whichever INIT_EVENT_DEFAULT
// finds).
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
// request_count. SO_REUSEADDR is set on the listener (like asio's acceptor),
// so back-to-back runs in the thread sweep don't hit EADDRINUSE. The
// listener is bound synchronously before the client starts, so there is no
// startup race.

#include "memusage.hpp"
#include "photon_bench.hpp"

#include <photon/net/socket.h>
#include <photon/photon.h>
#include <photon/thread/thread11.h>

#include <sys/socket.h>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace net = photon::net;

static constexpr std::uint16_t kPort = 55550;
static std::size_t request_count = 100000;
static std::size_t connection_count = 20;

static const std::string static_response = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!)";

static const std::string static_request =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

static void server_handler(net::ISocketStream* stream, std::size_t* served) {
  char data[4096];
  std::size_t i = 0;
  for (;;) {
    const ssize_t n = stream->recv(data, sizeof(data));
    if (n <= 0) {
      break; // client disconnected (or connection error; the served-request
             // total will catch undercounts)
    }
    const ssize_t w =
      stream->write(static_response.data(), static_response.size());
    if (w != static_cast<ssize_t>(static_response.size())) {
      break;
    }
    ++i;
  }
  delete stream;
  *served = i;
}

static void server(std::promise<void>& ready) {
  photon::init(
    photon::INIT_EVENT_DEFAULT & ~photon::INIT_EVENT_SIGNAL,
    photon::INIT_IO_NONE
  );

  auto* listener = net::new_tcp_socket_server();
  listener->setsockopt<int>(SOL_SOCKET, SO_REUSEADDR, 1);
  if (listener->bind(net::EndPoint(net::IPAddr::V4Loopback(), kPort)) != 0 ||
      listener->listen() != 0) {
    std::printf("FAIL: server bind/listen failed\n");
    std::abort();
  }
  ready.set_value();

  std::vector<std::size_t> served(connection_count, 0);
  std::vector<photon::join_handle*> handlers;
  handlers.reserve(connection_count);
  for (std::size_t i = 0; i < connection_count; ++i) {
    net::ISocketStream* connection = listener->accept();
    if (connection == nullptr) {
      std::printf("FAIL in accept\n");
      std::abort();
    }
    handlers.push_back(photon::thread_enable_join(
      photon::thread_create11(server_handler, connection, &served[i])
    ));
  }

  std::size_t total = 0;
  for (std::size_t i = 0; i < connection_count; ++i) {
    photon::thread_join(handlers[i]);
    total += served[i];
  }
  if (total != request_count) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", request_count, total
    );
  }

  delete listener;
  photon::fini();
}

static void client_handler(net::ISocketClient* client, std::size_t count) {
  net::ISocketStream* s =
    client->connect(net::EndPoint(net::IPAddr::V4Loopback(), kPort));
  if (s == nullptr) {
    std::printf("FAIL in client: connect failed\n");
    std::abort();
  }

  char response_buf[4096];
  std::size_t done = 0;
  for (std::size_t k = 0; k < count; ++k) {
    const ssize_t w = s->write(static_request.data(), static_request.size());
    if (w != static_cast<ssize_t>(static_request.size())) {
      break;
    }
    const ssize_t n = s->recv(response_buf, sizeof(response_buf));
    if (n <= 0) {
      break;
    }
    ++done;
  }
  delete s; // closes the connection; the server handler sees EOF
  if (done != count) {
    std::printf("FAIL in client: finished early\n");
    std::abort();
  }
}

static void client() {
  auto* client = net::new_tcp_socket_client();
  const std::size_t per_task = request_count / connection_count;
  const std::size_t rem = request_count % connection_count;
  std::vector<photon::join_handle*> clients;
  clients.reserve(connection_count);
  for (std::size_t i = 0; i < connection_count; ++i) {
    const std::size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(photon::thread_enable_join(
      photon::thread_create11(client_handler, client, count)
    ));
  }
  for (auto* handle : clients) {
    photon::thread_join(handle);
  }
  delete client;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    connection_count = static_cast<std::size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    request_count = static_cast<std::size_t>(atoi(argv[2]));
  }

  bench::quiet_logs();
  photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);

  // Bind the listener (and signal readiness) before sending traffic,
  // or the client would fail immediately.
  std::promise<void> ready;
  std::thread server_thread(server, std::ref(ready));
  ready.get_future().wait();

  auto startTime = std::chrono::high_resolution_clock::now();
  client();
  server_thread.join();
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

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

  photon::fini();
}
