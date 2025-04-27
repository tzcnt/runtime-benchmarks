// A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/

#include <cstddef>
#include <thread>
#ifdef _WIN32
#include <SDKDDKVer.h>
#endif

#define TMC_IMPL

#include "tmc/asio/aw_asio.hpp"
#include "tmc/asio/ex_asio.hpp"
#include "tmc/channel.hpp"
#include "tmc/spawn_many.hpp"
#include "tmc/spawn_task.hpp"
#include "tmc/sync.hpp"
#include "tmc/task.hpp"

#include <asio/buffer.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using asio::ip::tcp;

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
  asio::error_code ec;
  size_t recv_count;
};

// not safe to accept rvalue reference
// have to accept value so that it gets moved when the coro is constructed
tmc::task<void>
server_handler(auto Socket, size_t Count, tmc::chan_tok<result> Results) {
  char data[4096];
  size_t i = 0;
  for (; i < Count; ++i) {
    auto d = asio::buffer(data);
    auto [error, n] = co_await Socket.async_read_some(d, tmc::aw_asio);
    if (error) {
      Results.post(result{error, i});
      Socket.close();
      co_return;
    }

    auto d2 = asio::buffer(static_response);
    std::tie(error, n) = co_await asio::async_write(Socket, d2, tmc::aw_asio);
    if (error) {
      Results.post(result{error, i});
      Socket.close();
      co_return;
    }
  }

  Socket.shutdown(tcp::socket::shutdown_both);
  Socket.close();
  Results.post(result{asio::error_code{}, i});
}

static tmc::task<void> server(tmc::ex_asio& ex, uint16_t Port) {
  // TODO Replace this with async barrier
  auto finished_chan = tmc::make_channel<result>();
  tcp::acceptor acceptor(ex, {tcp::v4(), Port});

  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto [error, sock] = co_await acceptor.async_accept(tmc::aw_asio);
    if (error) {
      std::terminate();
    }
    tmc::spawn(server_handler(std::move(sock), REQUEST_COUNT, finished_chan))
      .detach();
  }
  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto result = co_await finished_chan.pull();
    if (!result.has_value()) {
      std::terminate();
    }
    // if (result.value().ec) {
    //   auto msg = result.value().ec.message();
    //   std::printf("FAIL in server: %s\n", msg.c_str());
    // }
    total += result.value().recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static tmc::task<void>
client_handler(tmc::ex_asio& ex, uint16_t Port, size_t Count) {
  tcp::socket s(ex);
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
    std::printf("client finished early\n");
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
    // co_await client_handler(ex, Port, count);
  }
  co_await tmc::spawn_many(clients.begin(), clients.end());
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  tmc::ex_asio server_executor;
  server_executor.init();
  tmc::ex_asio client_executor;
  client_executor.init();
  std::printf("serving on http://localhost:%d/\n", PORT);
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
  std::printf("    duration: %zu us\n", totalTimeUs.count());
  std::printf(
    "    requests/sec: %zu\n", REQUEST_COUNT * 1000000 / totalTimeUs.count()
  );
}
