//! End-to-end tests: spawn the real `assets-server` binary on an ephemeral
//! port and talk to it with the same JSON/binary protocol as
//! `src/client/LazyFS/lazyfs.js`.

use futures_util::{SinkExt, StreamExt};
use std::path::Path;
use std::process::Stdio;
use tokio::io::AsyncBufReadExt;
use tokio::net::TcpStream;
use tokio_tungstenite::tungstenite::Message;

type WsStream = tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<TcpStream>>;

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
    let mut child = tokio::process::Command::new(env!("CARGO_BIN_EXE_assets-server"))
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
        if let Some(pos) = line.find("Listening on ws://") {
            let addr = &line[pos + "Listening on ws://".len()..];
            let port: u16 = addr.rsplit(':').next().unwrap().trim().parse().unwrap();
            // Keep draining the log pipe in the background: if it were closed
            // here, the child's next log write would fail with EPIPE.
            tokio::spawn(async move { while let Ok(Some(_)) = lines.next_line().await {} });
            return (port, ServerGuard { child });
        }
    }
    panic!("assets-server exited without reporting its bound port");
}

async fn connect(port: u16) -> WsStream {
    let (ws, _) = tokio_tungstenite::connect_async(format!("ws://127.0.0.1:{port}"))
        .await
        .unwrap();
    ws
}

fn make_assets(dir: &Path) {
    let assets = dir.join("assets");
    std::fs::create_dir_all(&assets).unwrap();
    // 250 bytes with chunk_size 100 -> chunk 0 and 1 full, chunk 2 partial.
    std::fs::write(assets.join("UI.nx"), expected_contents()).unwrap();
}

fn expected_contents() -> Vec<u8> {
    (0..250u32).map(|i| (i % 256) as u8).collect()
}

#[tokio::test]
async fn get_size_reports_size_and_version() {
    let dir = tempfile::tempdir().unwrap();
    make_assets(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_size", "file": "UI.nx"}"#.to_owned(),
    ))
    .await
    .unwrap();

    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!("expected text reply")
    };
    let value: serde_json::Value = serde_json::from_str(&reply).unwrap();
    assert_eq!(value["type"], "size");
    assert_eq!(value["file"], "UI.nx");
    assert_eq!(value["size"], 250);
    assert!(value["version"].as_i64().unwrap() > 0);
}

#[tokio::test]
async fn get_size_missing_file_returns_minus_one() {
    let dir = tempfile::tempdir().unwrap();
    std::fs::write(dir.path().join("placeholder.nx"), b"placeholder").unwrap();
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_size", "file": "Nope.nx"}"#.to_owned(),
    ))
    .await
    .unwrap();

    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!("expected text reply")
    };
    let value: serde_json::Value = serde_json::from_str(&reply).unwrap();
    assert_eq!(value["size"], -1);
    assert_eq!(value["version"], -1);
}

#[tokio::test]
async fn get_chunks_streams_binary_frames_then_done() {
    let dir = tempfile::tempdir().unwrap();
    make_assets(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_chunks", "file": "UI.nx", "start": 0, "end": 2, "chunk_size": 100}"#
            .to_owned(),
    ))
    .await
    .unwrap();

    let contents = expected_contents();
    for index in 0..3u32 {
        let Message::Binary(frame) = ws.next().await.unwrap().unwrap() else {
            panic!("expected binary chunk frame")
        };
        assert_eq!(&frame[0..4], &index.to_le_bytes());
        let name_len = frame[4] as usize;
        assert_eq!(&frame[5..5 + name_len], b"UI.nx");
        let data = &frame[5 + name_len..];
        let start = index as usize * 100;
        let end = (start + 100).min(contents.len());
        assert_eq!(data, &contents[start..end]);
    }

    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!("expected chunks_done")
    };
    let value: serde_json::Value = serde_json::from_str(&reply).unwrap();
    assert_eq!(value["type"], "chunks_done");
    assert_eq!(value["file"], "UI.nx");
    assert_eq!(value["start"], 0);
    assert_eq!(value["end"], 2);
}

#[tokio::test]
async fn get_chunk_single_request() {
    let dir = tempfile::tempdir().unwrap();
    make_assets(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_chunk", "file": "UI.nx", "index": 1, "chunk_size": 100}"#.to_owned(),
    ))
    .await
    .unwrap();
    let Message::Binary(frame) = ws.next().await.unwrap().unwrap() else {
        panic!("expected binary frame")
    };
    assert_eq!(u32::from_le_bytes(frame[0..4].try_into().unwrap()), 1);
    assert_eq!(&frame[5 + 5..], &expected_contents()[100..200]);
}

#[tokio::test]
async fn traversal_is_limited_to_basename() {
    // Directory components from the client are stripped; only the basename is
    // probed under the served directory, matching the old server.
    let dir = tempfile::tempdir().unwrap();
    std::fs::create_dir_all(dir.path().join("outside")).unwrap();
    std::fs::write(dir.path().join("outside/secret.nx"), b"classified").unwrap();
    std::fs::write(dir.path().join("placeholder.nx"), b"placeholder").unwrap();
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_chunk", "file": "../outside/secret.nx", "index": 0}"#.to_owned(),
    ))
    .await
    .unwrap();
    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        // Served would also be fine (basename exists nowhere under root),
        // but it must not read ../../outside/secret.nx.
        panic!("expected error, file is outside the served directory")
    };
    assert!(reply.contains("Chunk not found"));
}

#[tokio::test]
async fn invalid_json_and_unknown_type_return_errors() {
    let dir = tempfile::tempdir().unwrap();
    std::fs::write(dir.path().join("placeholder.nx"), b"placeholder").unwrap();
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text("this is not json".to_owned()))
        .await
        .unwrap();
    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!()
    };
    assert!(reply.contains("Invalid JSON"));

    ws.send(Message::Text(r#"{"type": "bogus"}"#.to_owned()))
        .await
        .unwrap();
    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!()
    };
    assert!(reply.contains("Unknown message type"));
}

#[tokio::test]
async fn binary_request_frames_are_accepted() {
    // The old server accepted binary frames carrying JSON as well.
    let dir = tempfile::tempdir().unwrap();
    make_assets(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Binary(
        br#"{"type": "get_size", "file": "UI.nx"}"#.to_vec(),
    ))
    .await
    .unwrap();
    let Message::Text(reply) = ws.next().await.unwrap().unwrap() else {
        panic!()
    };
    let value: serde_json::Value = serde_json::from_str(&reply).unwrap();
    assert_eq!(value["size"], 250);
}

#[tokio::test]
async fn pipelined_requests_keep_order() {
    let dir = tempfile::tempdir().unwrap();
    make_assets(dir.path());
    let (port, _guard) = spawn_server(dir.path()).await;
    let mut ws = connect(port).await;

    ws.send(Message::Text(
        r#"{"type": "get_size", "file": "UI.nx"}"#.to_owned(),
    ))
    .await
    .unwrap();
    ws.send(Message::Text(
        r#"{"type": "get_chunk", "file": "UI.nx", "index": 2, "chunk_size": 100}"#.to_owned(),
    ))
    .await
    .unwrap();

    let Message::Text(size_reply) = ws.next().await.unwrap().unwrap() else {
        panic!()
    };
    assert!(size_reply.contains("\"size\":250"));
    let Message::Binary(frame) = ws.next().await.unwrap().unwrap() else {
        panic!()
    };
    assert_eq!(&frame[5 + 5..], &expected_contents()[200..250]);
}
