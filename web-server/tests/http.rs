//! End-to-end HTTP tests: spawn the real `web-server` binary on an ephemeral
//! port and speak raw HTTP/1.1 to it.

use std::path::Path;
use std::process::Stdio;
use tokio::io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

/// RAII guard: kills the spawned server when the test ends.
struct ServerGuard {
    child: tokio::process::Child,
}

impl Drop for ServerGuard {
    fn drop(&mut self) {
        let _ = self.child.start_kill();
    }
}

/// Spawn the real binary with an ephemeral port (`--port 0`) and learn the
/// actual port from the "Listening on" line it logs to stderr.
async fn spawn_server(dir: &Path) -> (u16, ServerGuard) {
    let mut child = tokio::process::Command::new(env!("CARGO_BIN_EXE_web-server"))
        .args(["--port", "0", "--bind", "127.0.0.1"])
        .arg("--directory")
        .arg(dir)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .kill_on_drop(true)
        .spawn()
        .unwrap();

    let stderr = child.stderr.take().unwrap();
    let mut lines = tokio::io::BufReader::new(stderr).lines();
    while let Ok(Some(line)) = lines.next_line().await {
        if let Some(pos) = line.find("Listening on http://") {
            let addr = &line[pos + "Listening on http://".len()..];
            let port: u16 = addr.rsplit(':').next().unwrap().trim().parse().unwrap();
            // Keep draining the log pipe in the background: if it were closed
            // here, the child's next log write would fail with EPIPE.
            tokio::spawn(async move {
                while let Ok(Some(_)) = lines.next_line().await {}
            });
            return (port, ServerGuard { child });
        }
    }
    panic!("web-server exited without reporting its bound port");
}

/// Send a raw request and read the whole response.
///
/// `Connection: close` is injected so the server releases the socket and the
/// response can be read to EOF.
async fn request(port: u16, raw: &str) -> (String, Vec<u8>) {
    let raw = raw.replacen("\r\n\r\n", "\r\nConnection: close\r\n\r\n", 1);
    let mut stream = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
    stream.write_all(raw.as_bytes()).await.unwrap();
    let mut response = Vec::new();
    stream.read_to_end(&mut response).await.unwrap();
    let split = response
        .windows(4)
        .position(|w| w == b"\r\n\r\n")
        .map(|pos| pos + 4)
        .unwrap_or(response.len());
    let head = String::from_utf8_lossy(&response[..split]).into_owned();
    let body = response[split..].to_vec();
    (head, body)
}

fn make_site(dir: &Path) {
    let contents: Vec<u8> = (0..250u32).map(|i| (i % 256) as u8).collect();
    std::fs::write(dir.join("client.wasm"), &contents).unwrap();
    std::fs::write(dir.join("data.bin"), &contents).unwrap();
    std::fs::write(dir.join("my file.js"), b"console.log(1);\n").unwrap();
    let sub = dir.join("sub");
    std::fs::create_dir_all(&sub).unwrap();
    std::fs::write(sub.join("index.html"), b"<html>sub index</html>").unwrap();
}

#[tokio::test]
async fn full_get_has_isolation_headers_and_body() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, body) = request(port, "GET /data.bin HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 200 OK"));
    assert!(head.contains("Cross-Origin-Opener-Policy: same-origin"));
    assert!(head.contains("Cross-Origin-Embedder-Policy: require-corp"));
    assert!(head.contains("Access-Control-Allow-Origin: *"));
    assert!(head.contains("Cache-Control: no-store, no-cache, must-revalidate"));
    assert!(head.contains("Accept-Ranges: bytes"));
    assert!(head.contains("Content-Type: application/octet-stream"));
    assert!(head.contains("Content-Length: 250"));
    assert_eq!(body.len(), 250);
    assert_eq!(body[100], 100);
}

#[tokio::test]
async fn wasm_content_type_is_streamable() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let (head, _) = request(port, "GET /client.wasm HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.contains("Content-Type: application/wasm"));
}

#[tokio::test]
async fn range_requests_return_206() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, body) =
        request(port, "GET /data.bin HTTP/1.1\r\nHost: x\r\nRange: bytes=0-9\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 206 Partial Content"));
    assert!(head.contains("Content-Range: bytes 0-9/250"));
    assert_eq!(body, (0..10u8).collect::<Vec<_>>());

    let (head, body) =
        request(port, "GET /data.bin HTTP/1.1\r\nHost: x\r\nRange: bytes=100-\r\n\r\n").await;
    assert!(head.contains("Content-Range: bytes 100-249/250"));
    assert_eq!(body.len(), 150);

    let (_, body) =
        request(port, "GET /data.bin HTTP/1.1\r\nHost: x\r\nRange: bytes=-10\r\n\r\n").await;
    assert_eq!(body, (240..250u32).map(|i| i as u8).collect::<Vec<_>>());
}

#[tokio::test]
async fn bad_range_returns_416_with_total_size() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, body) =
        request(port, "GET /data.bin HTTP/1.1\r\nHost: x\r\nRange: bytes=abc\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 416"));
    assert!(head.contains("Content-Range: bytes */250"));
    assert!(body.is_empty());
}

#[tokio::test]
async fn missing_and_traversal_are_rejected() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    std::fs::write(dir.path().parent().unwrap().join("outside-secret"), b"no").unwrap();
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, _) = request(port, "GET /nope.bin HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 404"));

    let (head, _) = request(port, "GET /../outside-secret HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 403"));
}

#[tokio::test]
async fn head_has_length_but_no_body() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, body) = request(port, "HEAD /data.bin HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert!(head.contains("Content-Length: 250"));
    assert!(body.is_empty());
}

#[tokio::test]
async fn directory_serves_index_or_listing_and_redirects() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    // /sub redirects to /sub/
    let (head, _) = request(port, "GET /sub HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 301"));
    assert!(head.contains("Location: /sub/"));

    // /sub/ serves index.html
    let (head, body) = request(port, "GET /sub/ HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert_eq!(body, b"<html>sub index</html>");

    // Root listing links to entries
    let (head, body) = request(port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert!(head.contains("text/html"));
    let listing = String::from_utf8_lossy(&body);
    assert!(listing.contains("data.bin"));
    assert!(listing.contains("sub/"));
}

#[tokio::test]
async fn percent_encoded_and_queried_paths_resolve() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let (head, body) = request(
        port,
        "GET /my%20file.js?cache=1 HTTP/1.1\r\nHost: x\r\n\r\n",
    )
    .await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert!(head.contains("Content-Type: application/javascript"));
    assert_eq!(body, b"console.log(1);\n");
}

/// Read exactly one HTTP response (headers + Content-Length body).
async fn read_one_response(stream: &mut TcpStream) -> (String, Vec<u8>) {
    let mut buffer = Vec::new();
    let mut chunk = [0u8; 1024];
    let split = loop {
        let count = stream.read(&mut chunk).await.unwrap();
        assert!(count > 0, "connection closed before a full response");
        buffer.extend_from_slice(&chunk[..count]);
        if let Some(pos) = buffer.windows(4).position(|w| w == b"\r\n\r\n") {
            break pos + 4;
        }
    };
    let head = String::from_utf8_lossy(&buffer[..split]).into_owned();
    let content_length = head
        .lines()
        .find_map(|line| line.strip_prefix("Content-Length: "))
        .and_then(|value| value.trim().parse::<usize>().ok())
        .unwrap_or(0);
    while buffer.len() < split + content_length {
        let count = stream.read(&mut chunk).await.unwrap();
        assert!(count > 0, "connection closed mid-body");
        buffer.extend_from_slice(&chunk[..count]);
    }
    (head, buffer[split..].to_vec())
}

#[tokio::test]
async fn keep_alive_serves_two_requests_per_connection() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;

    let mut stream = TcpStream::connect(("127.0.0.1", port)).await.unwrap();
    stream
        .write_all(b"GET /data.bin HTTP/1.1\r\nHost: x\r\n\r\n")
        .await
        .unwrap();
    let (head, body) = read_one_response(&mut stream).await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert!(head.contains("Connection: keep-alive"));
    assert_eq!(body.len(), 250);

    stream
        .write_all(b"GET /data.bin HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        .await
        .unwrap();
    let (head, body) = read_one_response(&mut stream).await;
    assert!(head.starts_with("HTTP/1.1 200"));
    assert!(head.contains("Connection: close"));
    assert_eq!(body.len(), 250);
}

#[tokio::test]
async fn unsupported_method_is_405() {
    let dir = tempfile::tempdir().unwrap();
    make_site(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let (head, _) = request(port, "DELETE /data.bin HTTP/1.1\r\nHost: x\r\n\r\n").await;
    assert!(head.starts_with("HTTP/1.1 405"));
    assert!(head.contains("Allow: GET, HEAD"));
}
