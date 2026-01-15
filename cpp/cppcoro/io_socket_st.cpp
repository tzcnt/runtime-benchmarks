// 2 threads with separate event loops:
// Thread 1: A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/
// Thread 2: A client that sends a static request to the server
// and reads back data
// Thread 2 may initiate multiple parallel connections to the server

// cppcoro has a conflict with the linux macro
#ifdef linux
#undef linux
#endif

#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <sys/socket.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace cppcoro;
using cppcoro::net::ip_endpoint;
using cppcoro::net::ipv4_address;
using cppcoro::net::ipv4_endpoint;

const uint16_t PORT = 55550;
static size_t REQUEST_COUNT = 100000;
static size_t CONNECTION_COUNT = 20;

const std::string static_response = R"(HTTP/1.1 200 OK
Content-Length: 12
Content-Type: text/plain; charset=utf-8

Hello World!)";

const std::string static_request =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

struct result {
  size_t recv_count;
};

task<result> server_handler(io_service& ioSvc, net::socket listeningSocket) {
  auto sock = net::socket::create_tcpv4(ioSvc);
  co_await listeningSocket.accept(sock);

  char data[4096];
  for (size_t i = 0;; ++i) {
    auto n = co_await sock.recv(data, sizeof(data));
    if (n == 0) {
      sock.close();
      co_return result{i};
    }

    size_t bytesSent = 0;
    while (bytesSent < static_response.size()) {
      bytesSent += co_await sock.send(
        static_response.data() + bytesSent, static_response.size() - bytesSent
      );
    }
  }
}

task<void> server(io_service& ioSvc, net::socket listeningSocket) {
  size_t per_handler = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;

  std::vector<task<result>> handlers;
  handlers.reserve(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_handler + 1 : per_handler;
    handlers.push_back(server_handler(ioSvc, listeningSocket));
  }

  auto results = co_await when_all(std::move(handlers));

  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    total += results[i].recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

task<void>
client_handler(io_service& ioSvc, ip_endpoint serverAddr, size_t Count) {
  auto s = net::socket::create_tcpv4(ioSvc);
  s.bind(ipv4_endpoint{});
  co_await s.connect(serverAddr);

  char response_buf[4096];
  for (size_t i = 0; i < Count; ++i) {
    size_t bytesSent = 0;
    while (bytesSent < static_request.size()) {
      bytesSent += co_await s.send(
        static_request.data() + bytesSent, static_request.size() - bytesSent
      );
    }
    auto n = co_await s.recv(response_buf, sizeof(response_buf));
    if (n == 0) {
      s.close();
      std::printf("FAIL in client: finished early\n");
      std::terminate();
    }
  }

  s.close_recv();
  s.close_send();
  s.close();
}

task<void> client(io_service& ioSvc, ip_endpoint serverAddr) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  std::vector<task<void>> clients;
  clients.reserve(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(client_handler(ioSvc, serverAddr, count));
  }
  co_await when_all(std::move(clients));
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }

  io_service serverIoSvc;
  io_service clientIoSvc;

  auto listeningSocket = net::socket::create_tcpv4(serverIoSvc);

  // For some reason this requires setting SO_REUSEADDR on Linux or else the
  // port becomes locked after the program exits. The other libraries don't have
  // this issue.
  int one = 1;
#ifdef _WIN32
  setsockopt(
    acceptor.native_handle(), SOL_SOCKET, SO_REUSEADDR,
    reinterpret_cast<const char*>(&one), sizeof(one)
  );
  ::setsockopt(
    listeningSocket.native_handle(), SOL_SOCKET, SO_REUSEADDR,
    reinterpret_cast<const char*>(&one), sizeof(one)
  );
#else
  ::setsockopt(
    listeningSocket.native_handle(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)
  );
#endif
  listeningSocket.bind(ipv4_endpoint{ipv4_address::loopback(), PORT});
  listeningSocket.listen(static_cast<uint32_t>(CONNECTION_COUNT));

  auto serverAddr = listeningSocket.local_endpoint();

  auto serverTask = server(serverIoSvc, std::move(listeningSocket));

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  std::thread serverThread([&] { serverIoSvc.process_events(); });
  std::thread clientThread([&] { clientIoSvc.process_events(); });

  auto startTime = std::chrono::high_resolution_clock::now();
  sync_wait([&]() -> task<void> {
    co_await when_all(std::move(serverTask), client(clientIoSvc, serverAddr));
  }());

  serverIoSvc.stop();
  clientIoSvc.stop();
  serverThread.join();
  clientThread.join();

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
}
