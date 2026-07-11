// TCP ping-pong between a client and a server over the .NET stdlib async socket
// stack (Socket + the runtime's epoll-based SocketAsyncEngine).
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/io_socket_st.cpp
//
// Server: accepts connectionCount connections and, per connection, loops
//   reading a request and writing a fixed HTTP response until the client
//   disconnects.
// Client: opens connectionCount connections and sends requestCount requests
//   total (split across connections), reading the response each time.
//
// Argument convention matches the C++/Go versions: args[0] = connectionCount
// (build_and_bench_all.py fills this with the thread count), args[1] =
// requestCount. SO_REUSEADDR is set (asio/Go set it by default) so back-to-back
// runs in the thread sweep don't hit EADDRINUSE.

using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;
using BenchUtil;

namespace IoSocketSt;

internal static class Program
{
    private const int Port = 55550;

    private static readonly byte[] StaticResponse = Encoding.ASCII.GetBytes(
        "HTTP/1.1 200 OK\nContent-Length: 12\nContent-Type: text/plain; charset=utf-8\n\nHello World!");
    private static readonly byte[] StaticRequest = Encoding.ASCII.GetBytes(
        "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n");

    // Sends the whole buffer (asio's async_write / Go's Write both fully drain).
    private static async ValueTask SendAllAsync(Socket sock, ReadOnlyMemory<byte> data)
    {
        int sent = 0;
        while (sent < data.Length)
            sent += await sock.SendAsync(data[sent..], SocketFlags.None);
    }

    private static async Task<int> ServerHandler(Socket sock)
    {
        var buf = new byte[4096];
        int i = 0;
        try
        {
            while (true)
            {
                int n = await sock.ReceiveAsync(buf, SocketFlags.None);
                if (n == 0) // client disconnected
                    break;
                await SendAllAsync(sock, StaticResponse);
                i++;
            }
        }
        catch (SocketException)
        {
            // client hung up mid-exchange; report what completed
        }
        sock.Close();
        return i;
    }

    private static async Task Server(Socket listener, int connectionCount, int requestCount)
    {
        var handlers = new Task<int>[connectionCount];
        for (int idx = 0; idx < connectionCount; idx++)
        {
            Socket conn = await listener.AcceptAsync();
            handlers[idx] = ServerHandler(conn);
        }
        int[] counts = await Task.WhenAll(handlers);

        int total = 0;
        foreach (int c in counts)
            total += c;
        if (total != requestCount)
            Console.WriteLine($"FAIL: expected {requestCount} requests but served {total}");
    }

    private static async Task ClientHandler(EndPoint ep, int count)
    {
        var sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        await sock.ConnectAsync(ep);
        var buf = new byte[4096];
        int done = 0;
        for (int k = 0; k < count; k++)
        {
            await SendAllAsync(sock, StaticRequest);
            int n = await sock.ReceiveAsync(buf, SocketFlags.None);
            if (n == 0)
                break;
            done++;
        }
        if (done != count)
        {
            Console.WriteLine("FAIL in client: finished early");
            Environment.Exit(1);
        }
        sock.Shutdown(SocketShutdown.Both);
        sock.Close();
    }

    private static async Task Client(EndPoint ep, int connectionCount, int requestCount)
    {
        int perTask = requestCount / connectionCount;
        int rem = requestCount % connectionCount;
        var tasks = new Task[connectionCount];
        for (int i = 0; i < connectionCount; i++)
        {
            int count = perTask + (i < rem ? 1 : 0);
            tasks[i] = ClientHandler(ep, count);
        }
        await Task.WhenAll(tasks);
    }

    private static async Task<int> Main(string[] args)
    {
        // This is the "single-threaded" I/O benchmark: the reference runtimes run
        // a single-threaded event loop for the server and another for the client
        // (two OS threads total). Pin the ThreadPool to two worker threads to
        // match that budget, independent of the connection count in args[0].
        // (.NET's epoll SocketAsyncEngine runs its own poll thread, as Go's
        // netpoller does, so the true thread count is a touch higher on both.)
        Bench.ConfigureThreads(2);

        int connectionCount = 20;
        if (args.Length > 0 && int.TryParse(args[0], out int c) && c > 0)
            connectionCount = c;
        int requestCount = 100000;
        if (args.Length > 1 && int.TryParse(args[1], out int r) && r > 0)
            requestCount = r;

        // Bind synchronously up front so the port is open before the client
        // connects (avoids the C++ version's startup sleep race).
        var listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        listener.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        var endpoint = new IPEndPoint(IPAddress.Loopback, Port);
        listener.Bind(endpoint);
        listener.Listen(connectionCount);

        Task serverTask = Server(listener, connectionCount, requestCount);

        var sw = Stopwatch.StartNew();
        await Client(endpoint, connectionCount, requestCount);
        await serverTask;
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        if (durUs < 1)
            durUs = 1;
        listener.Close();

        Console.WriteLine($"connections: {connectionCount}");
        Console.WriteLine("runs:");
        Console.WriteLine("  - iteration_count: 1");
        Console.WriteLine($"    requests: {requestCount}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    requests/sec: {(long)requestCount * 1_000_000 / durUs}");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
