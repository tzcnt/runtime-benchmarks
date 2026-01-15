// 2 threads with separate event loops:
// Thread 1: A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/
// Thread 2: A client that sends a static request to the server
// and reads back data
// Thread 2 may initiate multiple parallel connections to the server

#ifdef _WIN32
#include <SDKDDKVer.h>
#endif

#define TMC_IMPL

#include "tmc/asio/aw_asio.hpp"
#include "tmc/asio/ex_asio.hpp"
#include "tmc/fork_group.hpp"
#include "tmc/spawn_many.hpp"
#include "tmc/sync.hpp"
#include "tmc/task.hpp"

#ifdef TMC_USE_BOOST_ASIO
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

namespace asio = boost::asio;
using boost::system::error_code;
#else
#include <asio/basic_socket_acceptor.hpp>
#include <asio/basic_stream_socket.hpp>
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

using asio::error_code;
#endif

using asio::ip::tcp;
using executor_t = asio::io_context::executor_type;
using acceptor_t = asio::basic_socket_acceptor<tcp, executor_t>;
using socket_t = asio::basic_stream_socket<tcp, executor_t>;

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
  error_code ec;
  size_t recv_count;
};

tmc::task<result> server_handler(socket_t Socket) {
  char data[4096];
  for (size_t i = 0;; ++i) {
    auto d = asio::buffer(data);
    auto [error, n] = co_await Socket.async_read_some(d, tmc::aw_asio);
    if (error) {
      Socket.close();
      co_return result{error, i};
    }

    auto d2 = asio::buffer(static_response);
    std::tie(error, n) = co_await asio::async_write(Socket, d2, tmc::aw_asio);
    if (error) {
      Socket.close();
      co_return result{error, i};
    }
  }
}

static tmc::task<void> server(tmc::ex_asio& ex, uint16_t Port) {
  acceptor_t acceptor(ex, {tcp::v4(), Port});

  // Wait for CONNECTION_COUNT connections to be opened
  auto handlers = tmc::fork_group<result>(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto [error, sock] = co_await acceptor.async_accept(tmc::aw_asio);
    if (error) {
      std::printf("FAIL in accept: %s", error.message().c_str());
      std::terminate();
    }
    handlers.fork(server_handler(std::move(sock)));
  }
  // Wait for all handlers to complete and then count the results
  auto results = co_await std::move(handlers);

  auto eof = asio::error::make_error_code(asio::error::misc_errors::eof);
  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto& result = results[i];
    // Expect success (completed all requests) or EOF (client disconnected
    // first)
    if (result.ec && result.ec != eof) {
      auto msg = result.ec.message();
      std::printf("FAIL in server: %s\n", msg.c_str());
    }
    total += result.recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static tmc::task<void>
client_handler(tmc::ex_asio& ex, uint16_t Port, size_t Count) {
  socket_t s(ex);
  co_await s.async_connect({tcp::v4(), Port}, tmc::aw_asio);

  auto d = asio::buffer(static_request);
  char response_buf[4096];
  size_t i = 0;
  for (; i < Count; ++i) {
    auto r = asio::buffer(response_buf);
    {
      auto [err, n2] = co_await asio::async_write(s, d, tmc::aw_asio);
      if (err) {
        s.close();
        break;
      }
    }
    {
      auto [err, n] = co_await s.async_read_some(r, tmc::aw_asio);
      if (err) {
        s.close();
        break;
      }
    }
  }
  if (!s.is_open()) {
    std::printf("FAIL in client: finished early\n");
    std::terminate();
  }

  s.shutdown(tcp::socket::shutdown_both);
  s.close();
}

static tmc::task<void> client(tmc::ex_asio& ex, uint16_t Port) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  std::vector<tmc::task<void>> clients(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients[i] = client_handler(ex, Port, count);
  }
  co_await tmc::spawn_many(clients);
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }

  tmc::ex_asio server_executor;
  server_executor.init();
  tmc::ex_asio client_executor;
  client_executor.init();
  auto server_future =
    tmc::post_waitable(server_executor, server(server_executor, PORT));
  // Ensure that the socket is actually open before sending traffic,
  // or the client will fail immediately.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto startTime = std::chrono::high_resolution_clock::now();
  auto client_future =
    tmc::post_waitable(client_executor, client(client_executor, PORT));
  client_future.wait();
  server_future.wait();
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
