// 2 threads with separate event loops:
// Thread 1: A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/
// Thread 2: A client that sends a static request to the server
// and reads back data
// Thread 2 may initiate multiple parallel connections to the server

#include <cstddef>
#include <thread>
#ifdef _WIN32
#include <SDKDDKVer.h>
#endif

#define TMC_IMPL

#include <boost/cobalt.hpp>
#include <boost/cobalt/main.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <boost/cobalt/wait_group.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <utility>

using boost::asio::ip::tcp;
namespace cobalt = boost::cobalt;

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
  std::exception_ptr ec;
  size_t recv_count;
};

using token = cobalt::channel<result>;

cobalt::detached server_handler(auto Socket, size_t Count, token& Results) {
  char data[4096];
  result r;
  size_t i = 0;
  try {
    for (; i < Count; ++i) {
      auto d = boost::asio::buffer(data);
      auto n = co_await Socket.async_read_some(d, cobalt::use_op);

      auto d2 = boost::asio::buffer(static_response);
      auto n2 = co_await boost::asio::async_write(Socket, d2, cobalt::use_op);
    }
  } catch (...) {
    r.ec = std::current_exception();
    Socket.close();
  }
  r.recv_count = i;

  if (Socket.is_open()) {
    Socket.shutdown(tcp::socket::shutdown_both);
    Socket.close();
  }
  co_await Results.write(r);
}

cobalt::thread server(uint16_t Port) {
  cobalt::channel<result> finished_chan(CONNECTION_COUNT);
  tcp::acceptor acceptor(
    co_await boost::cobalt::this_coro::executor, {tcp::v4(), Port}
  );

  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto sock = co_await acceptor.async_accept(cobalt::use_op);
    server_handler(std::move(sock), REQUEST_COUNT, finished_chan);
  }
  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto result = co_await finished_chan.read();
    // Expect success (no exception) or EOF (client disconnected first)
    if (result.ec) {
      try {
        std::rethrow_exception(result.ec);
      } catch (const boost::system::system_error& e) {
        if (e.code() != boost::asio::error::make_error_code(
                          boost::asio::error::misc_errors::eof
                        )) {
          std::printf("FAIL in server: %s\n", e.what());
        }
      } catch (const std::exception& e) {
        std::printf("FAIL in server: %s\n", e.what());
      }
    }
    total += result.recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static cobalt::promise<void> client_handler(uint16_t Port, size_t Count) {
  tcp::socket s(co_await boost::cobalt::this_coro::executor);
  co_await s.async_connect({tcp::v4(), Port}, cobalt::use_op);

  auto d = boost::asio::buffer(static_request);
  char response_buf[4096];
  size_t i = 0;
  try {
    for (; i < Count; ++i) {
      auto r = boost::asio::buffer(response_buf);
      {
        auto n2 = co_await boost::asio::async_write(s, d, cobalt::use_op);
      }
      {
        auto n = co_await s.async_read_some(r, cobalt::use_op);
      }
    }
  } catch (std::exception& e) {
    s.close();
  }
  if (!s.is_open()) {
    std::printf("FAIL in client: finished early\n");
    std::terminate();
  }

  s.shutdown(tcp::socket::shutdown_both);
  s.close();
}

cobalt::thread client(uint16_t Port) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  boost::cobalt::wait_group clients;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients.push_back(client_handler(Port, count));
  }
  co_await clients.wait();
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }
  auto server_future = server(PORT);
  // Ensure that the socket is actually open before sending traffic,
  // or the client will fail immediately.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto startTime = std::chrono::high_resolution_clock::now();
  auto client_future = client(PORT);
  client_future.join();
  server_future.join();
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
