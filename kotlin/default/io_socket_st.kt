// TCP ping-pong between a client and a server driven by coroutines over the JDK's
// asynchronous (NIO.2) socket channels.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/io_socket_st.cpp
//
// Server: accepts connectionCount connections and, per connection, loops reading
// a request and writing a fixed HTTP response until the client disconnects.
// Client: opens connectionCount connections and sends requestCount requests
// total (split across connections), reading the response each time.
//
// NOTE: unlike the fork-join/channel benchmarks, this one does NOT use
// benchutil.newDispatcher (the framework's fork-join scheduler). Async I/O here
// is driven by a NIO.2 AsynchronousChannelGroup, whose completion-callback thread
// pool is the natural "event loop"; the fork-join scheduler plays no role. The
// C++ reference runs a single-threaded event loop for the server and another for
// the client (two OS threads total); here a single AsynchronousChannelGroup
// backed by a 2-thread pool drives all async I/O, and that same pool is used as
// the coroutine dispatcher, so the whole benchmark runs on two threads (matching
// the Go suite's GOMAXPROCS(2)). This file is therefore identical in the "fjp"
// and "default" variants. CompletionHandler callbacks are bridged to suspend
// functions, so no thread ever blocks on I/O.
//
// The argument convention matches the C++ version: args[0] = connectionCount
// (build_and_bench_all.py fills this with the thread count), args[1] =
// requestCount.
@file:JvmName("Main")

package io_socket_st

import benchutil.peakMemoryUsageKiB
import kotlinx.coroutines.async
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.suspendCancellableCoroutine
import java.net.InetSocketAddress
import java.net.SocketAddress
import java.net.StandardSocketOptions
import java.nio.ByteBuffer
import java.nio.channels.AsynchronousChannelGroup
import java.nio.channels.AsynchronousServerSocketChannel
import java.nio.channels.AsynchronousSocketChannel
import java.nio.channels.CompletionHandler
import java.util.concurrent.Executors
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

private const val PORT = 55550

private val RESPONSE_BYTES =
    "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nHello World!"
        .toByteArray(Charsets.US_ASCII)
private val REQUEST_BYTES =
    "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n"
        .toByteArray(Charsets.US_ASCII)

// --- CompletionHandler -> suspend bridges -------------------------------------

private suspend fun AsynchronousServerSocketChannel.acceptSuspend(): AsynchronousSocketChannel =
    suspendCancellableCoroutine { cont ->
        accept(null, object : CompletionHandler<AsynchronousSocketChannel, Unit?> {
            override fun completed(result: AsynchronousSocketChannel, attachment: Unit?) = cont.resume(result)
            override fun failed(exc: Throwable, attachment: Unit?) { cont.resumeWithException(exc) }
        })
    }

private suspend fun AsynchronousSocketChannel.connectSuspend(remote: SocketAddress): Unit =
    suspendCancellableCoroutine { cont ->
        connect(remote, null, object : CompletionHandler<Void?, Unit?> {
            override fun completed(result: Void?, attachment: Unit?) { cont.resume(Unit) }
            override fun failed(exc: Throwable, attachment: Unit?) { cont.resumeWithException(exc) }
        })
    }

private suspend fun AsynchronousSocketChannel.readSuspend(buf: ByteBuffer): Int =
    suspendCancellableCoroutine { cont ->
        read(buf, null, object : CompletionHandler<Int, Unit?> {
            override fun completed(result: Int, attachment: Unit?) { cont.resume(result) }
            override fun failed(exc: Throwable, attachment: Unit?) { cont.resumeWithException(exc) }
        })
    }

private suspend fun AsynchronousSocketChannel.writeSuspend(buf: ByteBuffer): Int =
    suspendCancellableCoroutine { cont ->
        write(buf, null, object : CompletionHandler<Int, Unit?> {
            override fun completed(result: Int, attachment: Unit?) { cont.resume(result) }
            override fun failed(exc: Throwable, attachment: Unit?) { cont.resumeWithException(exc) }
        })
    }

private suspend fun AsynchronousSocketChannel.writeFully(buf: ByteBuffer) {
    while (buf.hasRemaining()) {
        if (writeSuspend(buf) < 0) break
    }
}

// --- server -------------------------------------------------------------------

private suspend fun serverHandler(socket: AsynchronousSocketChannel): Int {
    val response = ByteBuffer.wrap(RESPONSE_BYTES)
    val data = ByteBuffer.allocate(4096)
    var i = 0
    try {
        while (true) {
            data.clear()
            val n = socket.readSuspend(data)
            if (n <= 0) break // client disconnected
            response.rewind()
            socket.writeFully(response)
            i++
        }
    } catch (e: Exception) {
        // client closed the connection first
    }
    try { socket.close() } catch (e: Exception) {}
    return i
}

private suspend fun server(
    acceptor: AsynchronousServerSocketChannel,
    connectionCount: Int,
    requestCount: Int,
) = coroutineScope {
    val handlers = (0 until connectionCount).map {
        val socket = acceptor.acceptSuspend()
        async { serverHandler(socket) }
    }
    val total = handlers.sumOf { it.await() }
    if (total != requestCount) {
        println("FAIL: expected $requestCount requests but served $total")
    }
}

// --- client -------------------------------------------------------------------

private suspend fun clientHandler(group: AsynchronousChannelGroup, remote: SocketAddress, count: Int) {
    val socket = AsynchronousSocketChannel.open(group)
    socket.connectSuspend(remote)
    val request = ByteBuffer.wrap(REQUEST_BYTES)
    val buf = ByteBuffer.allocate(4096)
    var done = 0
    try {
        for (k in 0 until count) {
            request.rewind()
            socket.writeFully(request)
            buf.clear()
            if (socket.readSuspend(buf) <= 0) break
            done++
        }
    } catch (e: Exception) {
        // fall through to the early-finish check
    }
    if (done != count) {
        println("FAIL in client: finished early")
    }
    try { socket.close() } catch (e: Exception) {}
}

private suspend fun client(
    group: AsynchronousChannelGroup,
    remote: SocketAddress,
    connectionCount: Int,
    requestCount: Int,
) = coroutineScope {
    val perTask = requestCount / connectionCount
    val rem = requestCount % connectionCount
    (0 until connectionCount).map { i ->
        val count = perTask + if (i < rem) 1 else 0
        async { clientHandler(group, remote, count) }
    }.forEach { it.await() }
}

fun main(args: Array<String>) {
    val connectionCount = args.getOrNull(0)?.toIntOrNull()?.takeIf { it > 0 } ?: 20
    val requestCount = args.getOrNull(1)?.toIntOrNull()?.takeIf { it > 0 } ?: 100000

    // A single 2-thread pool drives all async I/O and doubles as the coroutine
    // dispatcher: two threads total for both the server and client event loops.
    val executor = Executors.newFixedThreadPool(2)
    val group = AsynchronousChannelGroup.withThreadPool(executor)
    val dispatcher = executor.asCoroutineDispatcher()
    val remote = InetSocketAddress("127.0.0.1", PORT)

    // Bind synchronously up front so the port is open before the client connects
    // (avoids the C++ version's startup sleep race).
    val acceptor = AsynchronousServerSocketChannel.open(group)
    acceptor.setOption(StandardSocketOptions.SO_REUSEADDR, true)
    acceptor.bind(remote)

    runBlocking(dispatcher) {
        val serverJob = launch { server(acceptor, connectionCount, requestCount) }

        val start = System.nanoTime()
        val clientJob = launch { client(group, remote, connectionCount, requestCount) }
        clientJob.join()
        serverJob.join()
        val durUs = ((System.nanoTime() - start) / 1000).coerceAtLeast(1)

        println("connections: $connectionCount")
        println("runs:")
        println("  - iteration_count: 1")
        println("    requests: $requestCount")
        println("    duration: $durUs us")
        println("    requests/sec: ${requestCount.toLong() * 1_000_000 / durUs}")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }

    try { acceptor.close() } catch (e: Exception) {}
    group.shutdown()
    dispatcher.close()
}
