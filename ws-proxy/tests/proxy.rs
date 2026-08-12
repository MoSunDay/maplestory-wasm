//! End-to-end tests: real proxy accept loop + fake TCP game server + a real
//! WebSocket client, mirroring how the WASM client uses the proxy.

use futures_util::{SinkExt, StreamExt};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio_tungstenite::tungstenite::Message;

fn docker_map(enabled: bool, mapped_host: &str) -> ws_proxy::DockerMap {
    ws_proxy::DockerMap {
        enabled,
        mapped_host: mapped_host.to_owned(),
    }
}

/// Start the proxy on an ephemeral port and return that port.
async fn spawn_proxy(map: ws_proxy::DockerMap) -> u16 {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    tokio::spawn(ws_proxy::run(listener, map));
    port
}

/// Fake game server: optionally sends `hello` first, then echoes everything.
async fn spawn_game_server(hello: Option<Vec<u8>>, echo: bool) -> u16 {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    tokio::spawn(async move {
        loop {
            let Ok((mut socket, _)) = listener.accept().await else {
                break;
            };
            let hello = hello.clone();
            tokio::spawn(async move {
                if let Some(hello) = &hello {
                    if socket.write_all(hello).await.is_err() {
                        return;
                    }
                }
                if !echo {
                    return; // hang up right after the hello
                }
                let mut buffer = [0u8; 4096];
                loop {
                    match socket.read(&mut buffer).await {
                        Ok(0) => break,
                        Ok(count) => {
                            if socket.write_all(&buffer[..count]).await.is_err() {
                                break;
                            }
                        }
                        Err(_) => break,
                    }
                }
            });
        }
    });
    port
}

async fn connect_and_target(
    proxy_port: u16,
    target: &str,
    as_text: bool,
) -> tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>> {
    let url = format!("ws://127.0.0.1:{proxy_port}");
    let (mut ws, _) = tokio_tungstenite::connect_async(url).await.unwrap();
    let msg = if as_text {
        Message::Text(target.to_owned())
    } else {
        Message::Binary(target.as_bytes().to_vec())
    };
    ws.send(msg).await.unwrap();
    ws
}

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

/// True if the WebSocket connection ended (EOF, close frame, or reset).
async fn connection_ends(
    ws: &mut tokio_tungstenite::WebSocketStream<
        tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>,
    >,
) -> bool {
    match ws.next().await {
        None | Some(Err(_)) => true,
        Some(Ok(Message::Close(_))) => true,
        Some(Ok(other)) => panic!("expected connection end, got {other:?}"),
    }
}
