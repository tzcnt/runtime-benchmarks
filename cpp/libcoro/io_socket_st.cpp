// A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/

#include "coro/coro.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
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
  coro::net::send_status sc;
  coro::net::recv_status rc;
  size_t recv_count;
};

// not safe to accept rvalue reference
// have to accept value so that it gets moved when the coro is constructed
coro::task<void> server_handler(
  coro::net::tcp::client client, size_t Count, coro::queue<result>& Results
) {
  // Why a string? IDK but this is what libcoro's examples prefer
  std::string data(4096, '\0');
  size_t i = 0;
  for (; i < Count; ++i) {
    while (true) {
      // Wait for data to be available to read.
      co_await client.poll(coro::poll_op::read);
      auto [rstatus, rspan] = client.recv(data);
      switch (rstatus) {
      case coro::net::recv_status::ok:
        goto SEND;
      case coro::net::recv_status::try_again:
        break;
      default:
        co_await Results.emplace(coro::net::send_status::ok, rstatus, i);
        co_return;
      }
    }
  SEND:
    // Make sure the client socket can be written to.
    co_await client.poll(coro::poll_op::write);
    auto sspan = std::span<const char>{static_response};
    coro::net::send_status sstatus;
    do {
      std::tie(sstatus, sspan) = client.send(sspan);
      if (sstatus != coro::net::send_status::ok) {
        co_await Results.emplace(sstatus, coro::net::recv_status::ok, i);
        co_return;
      }
    } while (!sspan.empty());
  }

  client.socket().shutdown();
  client.socket().close();
  co_await Results.emplace(
    coro::net::send_status::ok, coro::net::recv_status::ok, i
  );
}

static coro::task<void>
server(std::shared_ptr<coro::io_scheduler> executor, uint16_t Port) {
  executor->schedule();
  // TODO Replace this with async barrier
  auto finished_chan = coro::queue<result>();
  coro::net::tcp::server server{
    executor, coro::net::tcp::server::options{.port = Port}
  };

  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {

    // Wait for a new connection.
    auto pstatus = co_await server.poll();
    switch (pstatus) {
    case coro::poll_status::event: {
      auto client = server.accept();
      if (client.socket().is_valid()) {
        executor->spawn_detached(make_on_connection_task(std::move(client)));
      } // else report error or something if the socket was invalid or could not
        // be accepted.
    } break;
    case coro::poll_status::write:
    case coro::poll_status::error:
    case coro::poll_status::closed:
    case coro::poll_status::timeout:
    default:
      co_return;
    }

    auto [error, sock] = co_await acceptor.async_accept(coro::aw_asio);
    if (error) {
      std::terminate();
    }
    coro::spawn(server_handler(std::move(sock), REQUEST_COUNT, finished_chan))
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

static coro::task<void>
client_handler(coro::ex_asio& ex, uint16_t Port, size_t Count) {
  tcp::socket s(ex);
  co_await s.async_connect({tcp::v4(), Port}, coro::aw_asio);

  auto d = boost::asio::buffer(static_request);
  char response_buf[4096];
  size_t i = 0;
  for (; i < Count; ++i) {
    auto r = boost::asio::buffer(response_buf);
    {
      auto [err, n2] = co_await boost::asio::async_write(s, d, coro::aw_asio);
      if (err) {
        s.close();
        break;
      }
    }
    {
      auto [err, n] = co_await s.async_read_some(r, coro::aw_asio);
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

static coro::task<void> client(coro::ex_asio& ex, uint16_t Port) {
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  std::vector<coro::task<void>> clients(CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    clients[i] = client_handler(ex, Port, count);
    // co_await client_handler(ex, Port, count);
  }
  co_await coro::spawn_many(clients.begin(), clients.end());
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }

  auto server_executor = coro::io_scheduler::make_shared(
    coro::io_scheduler::options{
      .execution_strategy =
        coro::io_scheduler::execution_strategy_t::process_tasks_inline
    }
  );

  auto client_executor = coro::io_scheduler::make_shared(
    coro::io_scheduler::options{
      .execution_strategy =
        coro::io_scheduler::execution_strategy_t::process_tasks_inline
    }
  );

  std::printf("serving on http://localhost:%d/\n", PORT);

  auto startTime = std::chrono::high_resolution_clock::now();
  coro::sync_wait(
    coro::when_all(server(server_executor), client(client_executor))
  );
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("    duration: %zu us\n", totalTimeUs.count());
  std::printf(
    "    requests/sec: %zu\n", REQUEST_COUNT * 1000000 / totalTimeUs.count()
  );
}
