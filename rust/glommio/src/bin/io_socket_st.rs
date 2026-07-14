// TCP ping-pong between a single-threaded client and a single-threaded server,
// each on its own glommio LocalExecutor / OS thread.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/io_socket_st.cpp
//
// Thread 1 (server): accepts CONNECTION_COUNT connections and, per connection,
//   loops reading a request and writing a fixed HTTP response until the client
//   disconnects.
// Thread 2 (client): opens CONNECTION_COUNT connections and sends REQUEST_COUNT
//   requests total (split across connections), reading the response each time.
//
// glommio is a thread-per-core, io_uring-only runtime: one single-threaded
// LocalExecutor drives all CONNECTION_COUNT connections on a given thread,
// multiplexed as !Send tasks via glommio::spawn_local. That maps exactly onto
// io_socket_st (the "_st" = single-threaded), whose server and client are each a
// single event loop on a separate OS thread. glommio's TcpStream implements the
// standard futures AsyncRead/AsyncWrite, so read/write_all come from
// futures_lite's ext traits (readiness-style borrowed buffers, like smol/tokio).
//
// The argument convention matches the C++ version: argv[1] = CONNECTION_COUNT
// (build_and_bench_all.py fills this with the thread count), argv[2] =
// REQUEST_COUNT.

use futures_lite::io::{AsyncReadExt, AsyncWriteExt};
use glommio::net::{TcpListener, TcpStream};
use glommio::{LocalExecutorBuilder, Placement};
use glommio_bench::peak_memory_usage;
use std::net::Shutdown;
use std::os::unix::io::{FromRawFd, IntoRawFd};
use std::time::Instant;

const PORT: u16 = 55550;

const STATIC_RESPONSE: &str = "HTTP/1.1 200 OK\nContent-Length: 12\nContent-Type: text/plain; charset=utf-8\n\nHello World!";
const STATIC_REQUEST: &str = "HEAD / HTTP/1.1\r\nHost: host:port\r\nConnection: close\r\n\r\n";

async fn server_handler(mut socket: TcpStream) -> usize {
    let mut data = [0u8; 4096];
    let mut i = 0usize;
    loop {
        match socket.read(&mut data).await {
            Ok(0) | Err(_) => return i, // client disconnected / error
            Ok(_) => {}
        }
        if socket.write_all(STATIC_RESPONSE.as_bytes()).await.is_err() {
            return i;
        }
        i += 1;
    }
}

async fn server(listener: TcpListener, connection_count: usize, request_count: usize) {
    let mut handlers = Vec::with_capacity(connection_count);
    for _ in 0..connection_count {
        let socket = listener.accept().await.expect("accept");
        handlers.push(glommio::spawn_local(server_handler(socket)));
    }

    let mut total = 0usize;
    for h in handlers {
        total += h.await;
    }
    if total != request_count {
        println!(
            "FAIL: expected {} requests but served {}",
            request_count, total
        );
    }
}

async fn client_handler(count: usize) {
    let mut s = TcpStream::connect(("127.0.0.1", PORT))
        .await
        .expect("connect");
    let mut response_buf = [0u8; 4096];
    let mut done = 0usize;
    for _ in 0..count {
        if s.write_all(STATIC_REQUEST.as_bytes()).await.is_err() {
            break;
        }
        if s.read(&mut response_buf).await.is_err() {
            break;
        }
        done += 1;
    }
    if done != count {
        println!("FAIL in client: finished early");
        std::process::exit(1);
    }
    let _ = s.shutdown(Shutdown::Both).await;
}

async fn client(connection_count: usize, request_count: usize) {
    let per_task = request_count / connection_count;
    let rem = request_count % connection_count;
    let mut clients = Vec::with_capacity(connection_count);
    for i in 0..connection_count {
        let count = if i < rem { per_task + 1 } else { per_task };
        clients.push(glommio::spawn_local(client_handler(count)));
    }
    for c in clients {
        c.await;
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let connection_count: usize = if args.len() > 1 {
        args[1].parse().expect("connection count")
    } else {
        20
    };
    let request_count: usize = if args.len() > 2 {
        args[2].parse().expect("request count")
    } else {
        100_000
    };

    // Bind synchronously up front so the port is guaranteed open before the
    // client starts connecting (avoids the C++ version's startup sleep race).
    // SO_REUSEADDR mirrors asio's acceptor default and prevents EADDRINUSE when
    // the harness runs this benchmark repeatedly across a thread sweep. glommio's
    // own TcpListener::bind sets only SO_REUSEPORT (not SO_REUSEADDR) and must
    // run inside an executor, so we bind here and adopt the raw fd via
    // FromRawFd inside the server executor (from_raw_fd needs the reactor
    // context, so it happens inside run()).
    let addr: std::net::SocketAddr = ([127, 0, 0, 1], PORT).into();
    let socket = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::STREAM,
        Some(socket2::Protocol::TCP),
    )
    .expect("socket");
    socket.set_reuse_address(true).expect("reuse_address");
    socket.bind(&addr.into()).expect("bind");
    socket.listen(1024).expect("listen");
    let std_listener: std::net::TcpListener = socket.into();
    let listener_fd = std_listener.into_raw_fd();

    let server_thread = std::thread::spawn(move || {
        let ex = LocalExecutorBuilder::new(Placement::Unbound)
            .make()
            .expect("make server executor");
        ex.run(async move {
            // SAFETY: listener_fd is an owned, already-bound-and-listening fd
            // handed off exactly once from main(); glommio takes ownership here.
            let listener = unsafe { TcpListener::from_raw_fd(listener_fd) };
            server(listener, connection_count, request_count).await;
        });
    });

    let start = Instant::now();
    {
        let ex = LocalExecutorBuilder::new(Placement::Unbound)
            .make()
            .expect("make client executor");
        ex.run(client(connection_count, request_count));
    }
    server_thread.join().unwrap();
    let dur = start.elapsed();

    println!("connections: {}", connection_count);
    println!("runs:");
    println!("  - iteration_count: 1");
    println!("    requests: {}", request_count);
    println!("    duration: {} us", dur.as_micros());
    println!(
        "    requests/sec: {}",
        (request_count as u128) * 1_000_000 / dur.as_micros().max(1)
    );
    println!("    max_rss: {} KiB", peak_memory_usage());
}
