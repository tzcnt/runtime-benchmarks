// TCP ping-pong between a client and a server, each running on its own
// single-threaded neco runtime (the neco scheduler is per-OS-thread, so this
// matches the canonical structure of two event loops on two threads).
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/io_socket_st.cpp
//
// Server: accepts CONNECTION_COUNT connections and, per connection, loops
// reading a request and writing a fixed HTTP response until the client
// disconnects.
//
// Client: opens CONNECTION_COUNT connections and sends REQUEST_COUNT
// requests total (split across connections), reading the response each time.
//
// The argument convention matches the C++ version: argv[1] =
// CONNECTION_COUNT (build_and_bench_all.py fills this with the thread
// count), argv[2] = REQUEST_COUNT. neco_serve sets SO_REUSEADDR (like asio's
// acceptor), so back-to-back runs in the thread sweep don't hit EADDRINUSE.

#include "bench_util.h"
#include "neco.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDRESS "127.0.0.1:55550"

static size_t REQUEST_COUNT = 100000;
static size_t CONNECTION_COUNT = 20;

static const char static_response[] =
  "HTTP/1.1 200 OK\n"
  "Content-Length: 12\n"
  "Content-Type: text/plain; charset=utf-8\n"
  "\n"
  "Hello World!";

static const char static_request[] =
  "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

// Set once the server's listening socket is bound, so the client doesn't
// start connecting before the port is open (avoids a startup race).
static atomic_int server_ready;

static void server_handler(int argc, void *argv[]) {
  (void)argc;
  int conn = *(int *)argv[0];
  size_t *count = argv[1];
  neco_waitgroup *wg = argv[2];
  char data[4096];
  size_t i = 0;
  for (;;) {
    ssize_t n = neco_read(conn, data, sizeof(data));
    if (n <= 0) { // client disconnected / error
      break;
    }
    if (neco_write(conn, static_response, sizeof(static_response) - 1) < 0) {
      break;
    }
    i++;
  }
  close(conn);
  *count = i;
  neco_waitgroup_done(wg);
}

static void server_main(int argc, void *argv[]) {
  (void)argc;
  (void)argv;
  int ln = neco_serve("tcp", ADDRESS);
  if (ln < 0) {
    printf("FAIL in listen: %s\n", neco_strerror(ln));
    exit(1);
  }
  atomic_store(&server_ready, 1);

  size_t *counts = calloc(CONNECTION_COUNT, sizeof(size_t));
  neco_waitgroup wg;
  neco_waitgroup_init(&wg);
  neco_waitgroup_add(&wg, (int)CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; i++) {
    int conn = neco_accept(ln, 0, 0);
    if (conn < 0) {
      printf("FAIL in accept: %s\n", neco_strerror(conn));
      exit(1);
    }
    // server_handler copies conn at entry (started coroutines run
    // immediately), so reusing the local across iterations is safe.
    neco_start(server_handler, 3, &conn, &counts[i], &wg);
  }
  neco_waitgroup_wait(&wg);
  close(ln);

  size_t total = 0;
  for (size_t i = 0; i < CONNECTION_COUNT; i++) {
    total += counts[i];
  }
  free(counts);
  if (total != REQUEST_COUNT) {
    printf(
      "FAIL: expected %zu requests but served %zu\n", REQUEST_COUNT, total
    );
  }
}

static void *server_thread_main(void *arg) {
  (void)arg;
  neco_start(server_main, 0);
  return NULL;
}

static void client_handler(int argc, void *argv[]) {
  (void)argc;
  size_t count = *(size_t *)argv[0];
  neco_waitgroup *wg = argv[1];
  int fd = neco_dial("tcp", ADDRESS);
  if (fd < 0) {
    printf("FAIL in connect: %s\n", neco_strerror(fd));
    exit(1);
  }
  char buf[4096];
  size_t done = 0;
  for (size_t k = 0; k < count; k++) {
    if (neco_write(fd, static_request, sizeof(static_request) - 1) < 0) {
      break;
    }
    if (neco_read(fd, buf, sizeof(buf)) <= 0) {
      break;
    }
    done++;
  }
  if (done != count) {
    printf("FAIL in client: finished early\n");
    exit(1);
  }
  close(fd);
  neco_waitgroup_done(wg);
}

static void client_main(int argc, void *argv[]) {
  (void)argc;
  (void)argv;
  size_t per_task = REQUEST_COUNT / CONNECTION_COUNT;
  size_t rem = REQUEST_COUNT % CONNECTION_COUNT;
  size_t *counts = malloc(CONNECTION_COUNT * sizeof(size_t));
  neco_waitgroup wg;
  neco_waitgroup_init(&wg);
  neco_waitgroup_add(&wg, (int)CONNECTION_COUNT);
  for (size_t i = 0; i < CONNECTION_COUNT; i++) {
    counts[i] = i < rem ? per_task + 1 : per_task;
    neco_start(client_handler, 2, &counts[i], &wg);
  }
  neco_waitgroup_wait(&wg);
  free(counts);
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    int v = atoi(argv[1]);
    if (v > 0) {
      CONNECTION_COUNT = (size_t)v;
    }
  }
  if (argc > 2) {
    int v = atoi(argv[2]);
    if (v > 0) {
      REQUEST_COUNT = (size_t)v;
    }
  }

  pthread_t server_thread;
  if (pthread_create(&server_thread, NULL, server_thread_main, NULL) != 0) {
    printf("FAIL: could not create server thread\n");
    exit(1);
  }
  while (!atomic_load(&server_ready)) {
    usleep(1000);
  }

  int64_t start = bench_now_us();
  neco_start(client_main, 0); // blocks until all client coroutines finish
  pthread_join(server_thread, NULL);
  int64_t dur_us = bench_now_us() - start;
  if (dur_us < 1) {
    dur_us = 1;
  }

  printf("connections: %zu\n", CONNECTION_COUNT);
  printf("runs:\n");
  printf("  - iteration_count: 1\n");
  printf("    requests: %zu\n", REQUEST_COUNT);
  printf("    duration: %" PRIi64 " us\n", dur_us);
  printf(
    "    requests/sec: %" PRIi64 "\n",
    (int64_t)REQUEST_COUNT * 1000000 / dur_us
  );
  printf("    max_rss: %ld KiB\n", peak_memory_usage());
}
