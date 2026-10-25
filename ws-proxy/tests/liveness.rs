//! Downstream liveness tests: the proxy must reclaim sessions whose browser
//! vanished without a WebSocket close frame (killed tab/process), while never
//! dropping clients that keep answering pings.

mod common;

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use common::{docker_map, spawn_disconnect_observer, spawn_game_server, spawn_proxy_with_liveness};
use futures_util::{SinkExt, StreamExt};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::time::{sleep, timeout, Duration};
use tokio_tungstenite::tungstenite::Message;

use common::connect_and_target;

/// A browser killed without a close frame (raw socket, never read or written
/// again) must be reclaimed: after the liveness timeout the proxy closes the
/// upstream TCP connection and the proxy connection task finishes.
#[tokio::test]
async fn dead_browser_session_is_reclaimed_after_liveness_timeout() {
    let (game_port, disconnected) = spawn_disconnect_observer().await;
    let (proxy_port, proxy_task) = spawn_proxy_with_liveness(
        docker_map(false, ""),
        ws_proxy::LivenessConfig::from_secs(1, 2),
    )
    .await;

    // Complete the WebSocket handshake and the target dial, then go dark:
    // the raw socket stays open but is never read from or written to again —
    // exactly a browser killed without sending a close frame.
    let _silent_browser =
        raw_silent_browser_handshake(proxy_port, &format!("127.0.0.1:{game_port}")).await;

    // The session must outlive the first ping period (2s timeout > 1.5s)...
    let mut disconnected = disconnected;
    timeout(Duration::from_millis(1500), &mut disconnected)
        .await
        .expect_err("proxy dropped the session before the liveness timeout");
    // ...and be reclaimed shortly after the timeout expires (~2-3s).
    timeout(Duration::from_secs(5), disconnected)
        .await
        .expect("proxy left the target TCP open for a dead browser")
        .expect("disconnect observer task failed");
    timeout(Duration::from_secs(5), proxy_task)
        .await
        .expect("proxy connection task did not finish after reclaiming")
        .expect("proxy connection task panicked");
}

/// A client that keeps reading (and therefore keeps answering pings) stays
/// connected well past the liveness timeout, with data still flowing in both
/// directions.
#[tokio::test]
async fn alive_browser_survives_past_liveness_timeout() {
    let game_port = spawn_game_server(None, true).await;
    let (proxy_port, proxy_task) = spawn_proxy_with_liveness(
        docker_map(false, ""),
        ws_proxy::LivenessConfig::from_secs(1, 2),
    )
    .await;

    let ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    let (mut sink, mut stream) = ws.split();

    // Reading the stream drives tungstenite's automatic Pong replies to the
    // proxy's Pings; echoed binaries are collected for assertions.
    let echoes: Arc<Mutex<Vec<Vec<u8>>>> = Arc::new(Mutex::new(Vec::new()));
    let closed = Arc::new(AtomicBool::new(false));
    let reader = {
        let (echoes, closed) = (echoes.clone(), closed.clone());
        tokio::spawn(async move {
            while let Some(msg) = stream.next().await {
                match msg.unwrap() {
                    Message::Binary(bytes) => echoes.lock().unwrap().push(bytes),
                    Message::Close(_) => {
                        closed.store(true, Ordering::SeqCst);
                        break;
                    }
                    _ => {}
                }
            }
        })
    };

    let first = vec![1, 2, 3];
    sink.send(Message::Binary(first.clone())).await.unwrap();
    wait_for_echo_count(&echoes, 1).await;
    assert_eq!(echoes.lock().unwrap()[0], first);

    // Sit still for longer than the 2s liveness timeout while only the
    // proxy's pings (and our automatic pongs) flow.
    sleep(Duration::from_millis(3500)).await;
    assert!(
        !closed.load(Ordering::SeqCst),
        "proxy closed a live session"
    );
    assert!(!reader.is_finished(), "reader saw the connection end");
    assert!(!proxy_task.is_finished(), "proxy gave up on a live session");

    // Both directions must still work: send and get the echo back.
    let second = vec![9, 9, 9];
    sink.send(Message::Binary(second.clone())).await.unwrap();
    wait_for_echo_count(&echoes, 2).await;
    assert_eq!(echoes.lock().unwrap()[1], second);
}

/// Perform the WebSocket handshake and the target request over a raw TCP
/// socket, returning the bare socket (no WebSocket layer). Keeping it around
/// without ever reading or writing again simulates a browser that died
/// without sending a close frame.
async fn raw_silent_browser_handshake(proxy_port: u16, target: &str) -> tokio::net::TcpStream {
    let mut sock = tokio::net::TcpStream::connect(("127.0.0.1", proxy_port))
        .await
        .unwrap();

    // Any base64 of 16 bytes works as a key; the server only hashes it.
    let request = format!(
        "GET / HTTP/1.1\r\nHost: 127.0.0.1:{proxy_port}\r\nUpgrade: websocket\r\n\
         Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\
         Sec-WebSocket-Version: 13\r\n\r\n"
    );
    sock.write_all(request.as_bytes()).await.unwrap();

    // Read the 101 response headers byte-wise until the final blank line.
    let mut response = Vec::new();
    let mut byte = [0u8; 1];
    while !response.ends_with(b"\r\n\r\n") {
        sock.read_exact(&mut byte).await.unwrap();
        response.push(byte[0]);
    }
    assert!(
        response.starts_with(b"HTTP/1.1 101"),
        "handshake failed: {}",
        String::from_utf8_lossy(&response)
    );

    // One masked binary frame carrying the target address.
    let payload = target.as_bytes();
    let mask = [0x5Au8, 0xA5, 0x37, 0x13];
    let mut frame = vec![0x82, 0x80 | payload.len() as u8]; // FIN+binary, MASK+length
    frame.extend_from_slice(&mask);
    for (i, byte) in payload.iter().enumerate() {
        frame.push(byte ^ mask[i % 4]);
    }
    sock.write_all(&frame).await.unwrap();

    sock
}

/// Poll until at least `count` binary frames have been recorded.
async fn wait_for_echo_count(echoes: &Arc<Mutex<Vec<Vec<u8>>>>, count: usize) {
    for _ in 0..100 {
        if echoes.lock().unwrap().len() >= count {
            return;
        }
        sleep(Duration::from_millis(50)).await;
    }
    panic!(
        "expected {count} echoed frames, got {:?}",
        echoes.lock().unwrap()
    );
}
