// TCP ping-pong between a client and a server, each running on its own
// goroutine over the shared Go runtime / netpoller.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/io_socket_st.cpp
//
// Server: accepts connectionCount connections and, per connection, loops
//
//	reading a request and writing a fixed HTTP response until the client
//	disconnects.
//
// Client: opens connectionCount connections and sends requestCount requests
//
//	total (split across connections), reading the response each time.
//
// The argument convention matches the C++ version: os.Args[1] = connectionCount
// (build_and_bench_all.py fills this with the thread count), os.Args[2] =
// requestCount. Go's net.Listen sets SO_REUSEADDR by default (like asio's
// acceptor), so back-to-back runs in the thread sweep don't hit EADDRINUSE.
package main

import (
	"fmt"
	"net"
	"os"
	"runtime"
	"strconv"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

const port = 55550

const staticResponse = "HTTP/1.1 200 OK\nContent-Length: 12\nContent-Type: text/plain; charset=utf-8\n\nHello World!"
const staticRequest = "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n"

func serverHandler(conn net.Conn) int {
	defer conn.Close()
	resp := []byte(staticResponse)
	data := make([]byte, 4096)
	i := 0
	for {
		n, err := conn.Read(data)
		if err != nil || n == 0 { // client disconnected / error
			return i
		}
		if _, err := conn.Write(resp); err != nil {
			return i
		}
		i++
	}
}

func server(ln net.Listener, connectionCount, requestCount int) {
	counts := make([]int, connectionCount)
	var wg sync.WaitGroup
	for idx := 0; idx < connectionCount; idx++ {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Printf("FAIL in accept: %v\n", err)
			os.Exit(1)
		}
		wg.Go(func() {
			counts[idx] = serverHandler(conn)
		})
	}
	wg.Wait()

	total := 0
	for _, c := range counts {
		total += c
	}
	if total != requestCount {
		fmt.Printf("FAIL: expected %d requests but served %d\n", requestCount, total)
	}
}

func clientHandler(addr string, count int) {
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		fmt.Printf("FAIL in connect: %v\n", err)
		os.Exit(1)
	}
	req := []byte(staticRequest)
	buf := make([]byte, 4096)
	done := 0
	for k := 0; k < count; k++ {
		if _, err := conn.Write(req); err != nil {
			break
		}
		if _, err := conn.Read(buf); err != nil {
			break
		}
		done++
	}
	if done != count {
		fmt.Println("FAIL in client: finished early")
		os.Exit(1)
	}
	conn.Close()
}

func client(addr string, connectionCount, requestCount int) {
	perTask := requestCount / connectionCount
	rem := requestCount % connectionCount
	var wg sync.WaitGroup
	for i := 0; i < connectionCount; i++ {
		count := perTask
		if i < rem {
			count++
		}
		wg.Go(func() {
			clientHandler(addr, count)
		})
	}
	wg.Wait()
}

func main() {
	// This is the "single-threaded" I/O benchmark: the reference runtimes run a
	// single-threaded event loop for the server and another for the client (two
	// OS threads total). Pin the Go scheduler to two logical processors to match
	// that thread budget, independent of the connection count passed in argv[1].
	runtime.GOMAXPROCS(2)

	connectionCount := 20
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil && v > 0 {
			connectionCount = v
		}
	}
	requestCount := 100000
	if len(os.Args) > 2 {
		if v, err := strconv.Atoi(os.Args[2]); err == nil && v > 0 {
			requestCount = v
		}
	}

	// Bind synchronously up front so the port is guaranteed open before the
	// client starts connecting (avoids the C++ version's startup sleep race).
	addr := fmt.Sprintf("127.0.0.1:%d", port)
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		fmt.Printf("FAIL in listen: %v\n", err)
		os.Exit(1)
	}
	defer ln.Close()

	serverDone := make(chan struct{})
	go func() {
		server(ln, connectionCount, requestCount)
		close(serverDone)
	}()

	start := time.Now()
	client(addr, connectionCount, requestCount)
	<-serverDone
	durUs := time.Since(start).Microseconds()
	if durUs < 1 {
		durUs = 1
	}

	fmt.Printf("connections: %d\n", connectionCount)
	fmt.Println("runs:")
	fmt.Println("  - iteration_count: 1")
	fmt.Printf("    requests: %d\n", requestCount)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    requests/sec: %d\n", int64(requestCount)*1_000_000/durUs)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
