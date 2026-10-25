//! Shared helpers for the ws-proxy integration tests: fake game servers,
//! proxy spawning, and WebSocket client plumbing.

#![allow(dead_code)]

use futures_util::{SinkExt, StreamExt};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio_tungstenite::tungstenite::Message;

pub fn docker_map(enabled: bool, mapped_host: &str) -> ws_proxy::DockerMap {
    ws_proxy::DockerMap {
        enabled,
        mapped_host: mapped_host.to_owned(),
    }
}

/// Start the proxy on an ephemeral port with production-default liveness and
/// return that port.
pub async fn spawn_proxy(map: ws_proxy::DockerMap) -> u16 {
    spawn_proxy_with_liveness(map, ws_proxy::LivenessConfig::defaults())
        .await
        .0
}

/// Like [`spawn_proxy`], but with an explicit liveness config and returning
/// the connection task so tests can assert when the proxy gives up on a
/// session.
pub async fn spawn_proxy_with_liveness(
    map: ws_proxy::DockerMap,
    liveness: ws_proxy::LivenessConfig,
) -> (u16, tokio::task::JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let task = tokio::spawn(async move {
        let (stream, _) = listener.accept().await.unwrap();
        let ws = tokio_tungstenite::accept_async(stream).await.unwrap();
        ws_proxy::handle_connection(ws, &map, liveness).await;
    });
    (port, task)
}

/// Fake game server: optionally sends `hello` first, then echoes everything.
pub async fn spawn_game_server(hello: Option<Vec<u8>>, echo: bool) -> u16 {
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

/// Fake game server that reports when the proxy closes its TCP side.
pub async fn spawn_disconnect_observer() -> (u16, tokio::task::JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let observer = tokio::spawn(async move {
        let (mut socket, _) = listener.accept().await.unwrap();
        socket.write_all(&[0x5A; 16]).await.unwrap();
        let mut buffer = [0u8; 16];
        loop {
            match socket.read(&mut buffer).await {
                Ok(0) => break,
                Ok(_) => {}
                Err(err) => panic!("target TCP read failed before EOF: {err}"),
            }
        }
    });
    (port, observer)
}

/// Connect a real WebSocket client to the proxy and send the target address.
pub async fn connect_and_target(
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

/// True if the WebSocket connection ended (EOF, close frame, or reset).
pub async fn connection_ends(
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
