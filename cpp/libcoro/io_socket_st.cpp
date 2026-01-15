// 2 threads with separate event loops:
// Thread 1: A simple "Hello, World!" HTTP response server
// Listens on http://localhost:55550/
// Thread 2: A client that sends a static request to the server
// and reads back data
// Thread 2 may initiate multiple parallel connections to the server

#include "coro/coro.hpp" // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

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

coro::task<void>
server_handler(coro::net::tcp::client client, coro::queue<result>& Results) {
  // Why a string? IDK but this is what libcoro's examples prefer
  std::string data(4096, '\0');
  for (size_t i = 0;; ++i) {
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
}

static coro::task<void>
server(std::unique_ptr<coro::io_scheduler>& executor, uint16_t Port) {
  executor->schedule();
  auto finished_chan = coro::queue<result>();
  coro::net::tcp::server server{
    executor, coro::net::tcp::server::options{.port = Port}
  };

  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    // Wait for a new connection.
    auto pstatus = co_await server.poll();
    switch (pstatus) {
    case coro::poll_status::read: {
      auto client = server.accept();
      if (client.socket().is_valid()) {
        executor->spawn_detached(
          server_handler(std::move(client), finished_chan)
        );
      } else {
        std::printf("server acceptor socket was invalid!\n");
        std::terminate();
      }
    } break;
    case coro::poll_status::write:
    case coro::poll_status::error:
    case coro::poll_status::closed:
    case coro::poll_status::timeout:
    default:
      std::printf("server acceptor error!\n");
      std::terminate();
    }
  }
  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    auto result = co_await finished_chan.pop();
    if (!result.has_value()) {
      std::printf("results channel closed prematurely!\n");
      std::terminate();
    }
    if (result.value().sc != coro::net::send_status::ok) {
      auto err = result.value().sc;
      std::printf("FAIL in server send. error code: %d\n", err);
      std::terminate();
    }
    // Expect ok (completed all requests) or closed (client disconnected first)
    if (result.value().rc != coro::net::recv_status::ok &&
        result.value().rc != coro::net::recv_status::closed) {
      auto err = result.value().rc;
      std::printf("FAIL in server recv. error code: %d\n", err);
      std::terminate();
    }
    total += result.value().recv_count;
  }
  if (total != REQUEST_COUNT) {
    std::printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static coro::task<void> client_handler(
  std::unique_ptr<coro::io_scheduler>& executor, uint16_t Port, size_t Count
) {
  executor->schedule();
  coro::net::tcp::client client{
    executor, coro::net::tcp::client::options{.port = Port}
  };
  auto cstat = co_await client.connect();
  if (cstat != coro::net::connect_status::connected) {
    std::printf("failed to connect\n");
    std::terminate();
  }

  co_await client.poll(coro::poll_op::write);

  std::string request_data(static_request);
  std::string response_buf(4096, '\0');
  size_t i = 0;
  for (; i < Count; ++i) {
    // Send request
    co_await client.poll(coro::poll_op::write);
    auto sspan = std::span<const char>{request_data};
    coro::net::send_status sstatus;
    do {
      std::tie(sstatus, sspan) = client.send(sspan);
      if (sstatus != coro::net::send_status::ok) {
        client.socket().close();
        co_return;
      }
    } while (!sspan.empty());

    // Receive response
    while (true) {
      co_await client.poll(coro::poll_op::read);
      auto [rstatus, rspan] = client.recv(response_buf);
      switch (rstatus) {
      case coro::net::recv_status::ok:
        goto NEXT_ITER;
      case coro::net::recv_status::try_again:
        break;
      default:
        client.socket().close();
        co_return;
      }
    }
  NEXT_ITER:;
  }

  client.socket().shutdown();
  client.socket().close();
}

static coro::task<void>
client(std::unique_ptr<coro::io_scheduler>& executor, uint16_t Port) {
  for (size_t i = 0; i < CONNECTION_COUNT; ++i) {
    size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
    size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
    size_t count = i < rem ? per_task + 1 : per_task;
    executor->spawn_detached(client_handler(executor, Port, count));
  }
  co_return;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    CONNECTION_COUNT = static_cast<size_t>(atoi(argv[1]));
  }
  if (argc > 2) {
    REQUEST_COUNT = static_cast<size_t>(atoi(argv[2]));
  }

  auto server_executor = coro::io_scheduler::make_unique(
    coro::io_scheduler::options{
      .execution_strategy =
        coro::io_scheduler::execution_strategy_t::process_tasks_inline
    }
  );

  auto client_executor = coro::io_scheduler::make_unique(
    coro::io_scheduler::options{
      .execution_strategy =
        coro::io_scheduler::execution_strategy_t::process_tasks_inline
    }
  );

  auto startTime = std::chrono::high_resolution_clock::now();
  coro::sync_wait(
    coro::when_all(server(server_executor, PORT), client(client_executor, PORT))
  );
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
