// TCP ping-pong between a client and a server, each driven over the shared JVM
// virtual-thread scheduler and the JDK's socket poller.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/io_socket_st.cpp
//
// Server: accepts connectionCount connections and, per connection, loops
//   reading a request and writing a fixed HTTP response until the client
//   disconnects.
// Client: opens connectionCount connections and sends requestCount requests
//   total (split across connections), reading the response each time.
//
// The argument convention matches the C++/Go versions: args[0] = connectionCount
// (build_and_bench_all.py fills this with the thread count), args[1] =
// requestCount. This is the "single-threaded" I/O benchmark: the reference
// runtimes run one event-loop thread for the server and one for the client, so
// we pin the virtual-thread carrier pool to 2, independent of the connection
// count. Blocking socket reads/writes on virtual threads unmount their carrier
// (the JDK poller drives readiness), which is exactly the workload under test.
package bench;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public final class IoSocketSt {
  private static final int PORT = 55550;

  private static final String STATIC_RESPONSE =
    "HTTP/1.1 200 OK\nContent-Length: 12\nContent-Type: text/plain; charset=utf-8\n\nHello World!";
  private static final String STATIC_REQUEST =
    "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

  static int serverHandler(Socket conn) {
    int i = 0;
    try (Socket s = conn) {
      InputStream in = s.getInputStream();
      OutputStream out = s.getOutputStream();
      byte[] resp = STATIC_RESPONSE.getBytes(StandardCharsets.US_ASCII);
      byte[] data = new byte[4096];
      while (true) {
        int n = in.read(data);
        if (n <= 0) { // client disconnected / EOF
          return i;
        }
        out.write(resp);
        i++;
      }
    } catch (IOException e) {
      return i;
    }
  }

  static void server(ServerSocket ln, int connectionCount, int requestCount) {
    int[] counts = new int[connectionCount];
    Thread[] threads = new Thread[connectionCount];
    for (int idx = 0; idx < connectionCount; idx++) {
      final Socket conn;
      try {
        conn = ln.accept();
      } catch (IOException e) {
        System.out.printf("FAIL in accept: %s%n", e.getMessage());
        System.exit(1);
        return;
      }
      final int slot = idx;
      threads[idx] = Thread.startVirtualThread(() -> counts[slot] = serverHandler(conn));
    }
    Bench.joinAll(threads);

    int total = 0;
    for (int c : counts) {
      total += c;
    }
    if (total != requestCount) {
      System.out.printf("FAIL: expected %d requests but served %d%n", requestCount, total);
    }
  }

  static void clientHandler(String host, int port, int count) {
    int done = 0;
    try (Socket s = new Socket(host, port)) {
      OutputStream out = s.getOutputStream();
      InputStream in = s.getInputStream();
      byte[] req = STATIC_REQUEST.getBytes(StandardCharsets.US_ASCII);
      byte[] buf = new byte[4096];
      for (int k = 0; k < count; k++) {
        out.write(req);
        int n = in.read(buf);
        if (n <= 0) {
          break;
        }
        done++;
      }
    } catch (IOException e) {
      // fall through to the finished-early check
    }
    if (done != count) {
      System.out.println("FAIL in client: finished early");
      System.exit(1);
    }
  }

  static void client(String host, int port, int connectionCount, int requestCount) {
    int perTask = requestCount / connectionCount;
    int rem = requestCount % connectionCount;
    Thread[] threads = new Thread[connectionCount];
    for (int i = 0; i < connectionCount; i++) {
      final int count = i < rem ? perTask + 1 : perTask;
      threads[i] = Thread.startVirtualThread(() -> clientHandler(host, port, count));
    }
    Bench.joinAll(threads);
  }

  public static void main(String[] args) throws IOException {
    // Pin the carrier pool to two threads to mirror the reference runtimes' two
    // single-threaded event loops (server + client). Must run before the first
    // virtual thread is created.
    Bench.setParallelism(2);

    int connectionCount = 20;
    if (args.length > 0) {
      try {
        int v = Integer.parseInt(args[0].trim());
        if (v > 0) {
          connectionCount = v;
        }
      } catch (NumberFormatException e) {
        // keep default
      }
    }
    int requestCount = 100000;
    if (args.length > 1) {
      try {
        int v = Integer.parseInt(args[1].trim());
        if (v > 0) {
          requestCount = v;
        }
      } catch (NumberFormatException e) {
        // keep default
      }
    }

    // Bind synchronously up front so the port is guaranteed open before the
    // client starts connecting (avoids the C++ version's startup sleep race).
    // setReuseAddress mirrors the SO_REUSEADDR that Go / asio set by default, so
    // back-to-back runs in the thread sweep don't hit "Address already in use".
    String host = "127.0.0.1";
    ServerSocket ln = new ServerSocket();
    ln.setReuseAddress(true);
    ln.bind(new InetSocketAddress(host, PORT));

    final int conns = connectionCount;
    final int reqs = requestCount;
    try (ln) {
      Thread serverThread = Thread.startVirtualThread(() -> server(ln, conns, reqs));

      long start = System.nanoTime();
      client(host, PORT, connectionCount, requestCount);
      Bench.join(serverThread);
      long durUs = (System.nanoTime() - start) / 1000;
      if (durUs < 1) {
        durUs = 1;
      }

      System.out.println("connections: " + connectionCount);
      System.out.println("runs:");
      System.out.println("  - iteration_count: 1");
      System.out.println("    requests: " + requestCount);
      System.out.println("    duration: " + durUs + " us");
      System.out.println("    requests/sec: " + ((long) requestCount * 1_000_000 / durUs));
      System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
    }
  }
}
