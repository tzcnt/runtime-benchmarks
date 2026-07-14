# TCP ping-pong between a client and a server, each running a single-threaded
# std/asyncdispatch event loop on its own OS thread (two OS threads total).
#
# Canonical (TooManyCooks) implementation:
# ../../cpp/TooManyCooks/io_socket_st.cpp
#
# Weave (and weave-io) are compute threadpools with no socket support, so this
# benchmark uses Nim's native async I/O: std/asyncnet on the std/asyncdispatch
# (epoll/kqueue) event loop - the same "one single-threaded event loop per
# side" shape as the reference implementations. Sockets are unbuffered so that
# recv returns whatever the OS delivers, matching the reference read() usage.
#
# Server: accepts connectionCount connections and, per connection, loops
# reading a request and writing a fixed HTTP response until the client
# disconnects.
#
# Client: opens connectionCount connections and sends requestCount requests
# total (split across connections), reading the response each time.
#
# The argument convention matches the C++ version: paramStr(1) =
# connectionCount (build_and_bench_all.py fills this with the thread count),
# paramStr(2) = requestCount.

import std/[asyncdispatch, asyncnet, atomics, monotimes, net, os, strutils, times]
import benchutil

const port = Port(55550)

const staticResponse = "HTTP/1.1 200 OK\nContent-Length: 12\nContent-Type: text/plain; charset=utf-8\n\nHello World!"
const staticRequest = "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n"

# The server thread flips this after listen() succeeds so the client never
# connects before the port is open (avoids the C++ version's startup sleep).
var serverReady: Atomic[bool]

proc serverHandler(conn: AsyncSocket): Future[int] {.async.} =
  var served = 0
  while true:
    let data = await conn.recv(4096)
    if data.len == 0: # client disconnected / error
      break
    await conn.send(staticResponse)
    inc served
  conn.close()
  return served

proc serve(connectionCount, requestCount: int) {.async.} =
  let listener = newAsyncSocket(buffered = false)
  listener.setSockOpt(OptReuseAddr, true)
  listener.bindAddr(port, "127.0.0.1")
  listener.listen()
  serverReady.store(true)

  # Wait for connectionCount connections to be opened
  var handlers: seq[Future[int]] = @[]
  for _ in 0 ..< connectionCount:
    let conn = await listener.accept()
    handlers.add serverHandler(conn)

  var total = 0
  for h in handlers:
    total += await h
  listener.close()
  if total != requestCount:
    echo "FAIL: expected ", requestCount, " requests but served ", total

proc serverMain(cfg: tuple[connections, requests: int]) {.thread.} =
  waitFor serve(cfg.connections, cfg.requests)

proc clientHandler(count: int): Future[void] {.async.} =
  let conn = newAsyncSocket(buffered = false)
  await conn.connect("127.0.0.1", port)
  var done = 0
  for _ in 0 ..< count:
    await conn.send(staticRequest)
    let resp = await conn.recv(4096)
    if resp.len == 0:
      break
    inc done
  if done != count:
    echo "FAIL in client: finished early"
    quit(1)
  conn.close()

proc client(connectionCount, requestCount: int) {.async.} =
  let perTask = requestCount div connectionCount
  let rem = requestCount mod connectionCount
  var tasks: seq[Future[void]] = @[]
  for i in 0 ..< connectionCount:
    var count = perTask
    if i < rem:
      inc count
    tasks.add clientHandler(count)
  for t in tasks:
    await t

proc main() =
  var connectionCount = 20
  if paramCount() >= 1:
    try:
      let v = parseInt(paramStr(1))
      if v > 0:
        connectionCount = v
    except ValueError:
      discard
  var requestCount = 100000
  if paramCount() >= 2:
    try:
      let v = parseInt(paramStr(2))
      if v > 0:
        requestCount = v
    except ValueError:
      discard

  var serverThread: Thread[tuple[connections, requests: int]]
  createThread(serverThread, serverMain, (connectionCount, requestCount))
  while not serverReady.load():
    sleep(1)

  let start = getMonoTime()
  waitFor client(connectionCount, requestCount)
  joinThread(serverThread)
  var durUs = (getMonoTime() - start).inMicroseconds
  if durUs < 1:
    durUs = 1

  echo "connections: ", connectionCount
  echo "runs:"
  echo "  - iteration_count: 1"
  echo "    requests: ", requestCount
  echo "    duration: ", durUs, " us"
  echo "    requests/sec: ", int64(requestCount) * 1_000_000 div durUs
  echo "    max_rss: ", peakMemoryUsageKiB(), " KiB"

when isMainModule:
  main()
