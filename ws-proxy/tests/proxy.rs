//! End-to-end tests: real proxy accept loop + fake TCP game server + a real
//! WebSocket client, mirroring how the WASM client uses the proxy.
//!
//! Shared plumbing (fake servers, proxy spawning) lives in `tests/common`.

mod common;

use common::{
    connect_and_target, connection_ends, docker_map, spawn_disconnect_observer, spawn_game_server,
    spawn_proxy,
};
use futures_util::{SinkExt, StreamExt};
use tokio::net::TcpListener;
use tokio::time::{timeout, Duration};
use tokio_tungstenite::tungstenite::Message;

#[tokio::test]
async fn hello_arrives_as_single_16_byte_frame() {
    // The client's first ws_recv must return exactly the 16-byte handshake.
    let hello: Vec<u8> = (1..=16).collect();
    let game_port = spawn_game_server(Some(hello.clone()), true).await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    let first = ws.next().await.unwrap().unwrap();
    assert_eq!(first, Message::Binary(hello));
}

#[tokio::test]
async fn binary_loopback() {
    let game_port = spawn_game_server(None, true).await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    let payload: Vec<u8> = (0..200).map(|i| (i % 251) as u8).collect();
    ws.send(Message::Binary(payload.clone())).await.unwrap();

    let echoed = ws.next().await.unwrap().unwrap();
    assert_eq!(echoed, Message::Binary(payload));
}

#[tokio::test]
async fn zero_length_keepalive_does_not_change_tcp_payload() {
    let game_port = spawn_game_server(None, true).await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;
    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;

    ws.send(Message::Binary(Vec::new())).await.unwrap();
    let payload = vec![0x13, 0x37, 0x42];
    ws.send(Message::Binary(payload.clone())).await.unwrap();

    assert_eq!(ws.next().await.unwrap().unwrap(), Message::Binary(payload));
}

#[tokio::test]
async fn text_target_and_text_frames_dropped_after_handshake() {
    let game_port = spawn_game_server(None, true).await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    // Target sent as a text frame, like a non-standard client would.
    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), true).await;

    // Text after the handshake must be dropped, not forwarded to TCP.
    ws.send(Message::Text("not a game packet".to_owned()))
        .await
        .unwrap();

    let payload = vec![7u8, 8, 9, 10];
    ws.send(Message::Binary(payload.clone())).await.unwrap();

    // If the text frame had leaked through, the echo would contain it.
    let echoed = ws.next().await.unwrap().unwrap();
    assert_eq!(echoed, Message::Binary(payload));
}

#[tokio::test]
async fn invalid_target_closes_connection() {
    let proxy_port = spawn_proxy(docker_map(false, "")).await;
    let mut ws = connect_and_target(proxy_port, "definitely-not-a-target", false).await;
    assert!(connection_ends(&mut ws).await);
}

#[tokio::test]
async fn refused_target_closes_connection() {
    // Grab a port and drop the listener so connections to it are refused.
    let free_port = TcpListener::bind("127.0.0.1:0")
        .await
        .unwrap()
        .local_addr()
        .unwrap()
        .port();
    let proxy_port = spawn_proxy(docker_map(false, "")).await;
    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{free_port}"), false).await;
    assert!(connection_ends(&mut ws).await);
}

#[tokio::test]
async fn server_hangup_closes_client() {
    let game_port = spawn_game_server(Some(vec![0xAB; 16]), false).await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    assert_eq!(
        ws.next().await.unwrap().unwrap(),
        Message::Binary(vec![0xAB; 16])
    );
    assert!(connection_ends(&mut ws).await);
}

#[tokio::test]
async fn websocket_close_immediately_closes_target_tcp() {
    let (game_port, disconnected) = spawn_disconnect_observer().await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    assert_eq!(
        ws.next().await.unwrap().unwrap(),
        Message::Binary(vec![0x5A; 16])
    );

    ws.close(None).await.unwrap();
    timeout(Duration::from_secs(1), disconnected)
        .await
        .expect("proxy left the target TCP connection open")
        .expect("disconnect observer task failed");
}

#[tokio::test]
async fn websocket_transport_drop_immediately_closes_target_tcp() {
    let (game_port, disconnected) = spawn_disconnect_observer().await;
    let proxy_port = spawn_proxy(docker_map(false, "")).await;

    let mut ws = connect_and_target(proxy_port, &format!("127.0.0.1:{game_port}"), false).await;
    assert_eq!(
        ws.next().await.unwrap().unwrap(),
        Message::Binary(vec![0x5A; 16])
    );

    drop(ws);
    timeout(Duration::from_secs(1), disconnected)
        .await
        .expect("proxy left target TCP open after WebSocket transport drop")
        .expect("disconnect observer task failed");
}

#[tokio::test]
async fn docker_remap_routes_localhost_target() {
    // With remapping enabled, `localhost:<port>` must reach the echo server
    // via the mapped host (127.0.0.1 here).
    let game_port = spawn_game_server(Some(vec![0x5A; 16]), true).await;
    let proxy_port = spawn_proxy(docker_map(true, "127.0.0.1")).await;

    let mut ws = connect_and_target(proxy_port, &format!("localhost:{game_port}"), false).await;
    assert_eq!(
        ws.next().await.unwrap().unwrap(),
        Message::Binary(vec![0x5A; 16])
    );
}
