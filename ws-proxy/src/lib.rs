//! WebSocket-to-TCP proxy for the MapleStory WASM client.
//!
//! Browsers cannot open raw TCP sockets, so the WASM client opens a
//! WebSocket to this proxy instead. The first WebSocket message (text or
//! binary) carries the target address as `host:port`; everything after that
//! is forwarded verbatim between the WebSocket and a TCP connection to the
//! game server.

use std::io;
use std::net::Shutdown;

use futures_util::{SinkExt, StreamExt};
use socket2::SockRef;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::signal;
use tokio::task::JoinSet;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::WebSocketStream;
use tracing::{debug, info, warn};

/// Maximum bytes read from TCP per iteration. Each read becomes exactly one
/// binary WebSocket frame, which matters right after connecting: the game
/// server sends a 16-byte Hello packet and the client expects its first
/// WebSocket message to contain exactly those 16 bytes.
pub const TCP_READ_SIZE: usize = 64 * 1024;

/// Docker localhost remapping configuration, captured from the environment.
///
/// Inside Docker, a `localhost` target coming from the browser would resolve
/// to the container itself instead of the host machine, so loopback targets
/// are rewritten to a routable hostname (normally `host.docker.internal`).
pub struct DockerMap {
    pub enabled: bool,
    pub mapped_host: String,
}

impl DockerMap {
    pub fn from_env() -> Self {
        let enabled = std::env::var("IS_DOCKER")
            .map(|value| value.eq_ignore_ascii_case("true"))
            .unwrap_or(false);
        let mapped_host = std::env::var("WS_PROXY_LOCALHOST_TARGET")
            .unwrap_or_else(|_| "host.docker.internal".to_owned());
        DockerMap {
            enabled,
            mapped_host,
        }
    }
}

/// Rewrite loopback targets to the Docker-routable host when enabled.
pub fn remap_target_host<'a>(host: &'a str, map: &'a DockerMap) -> &'a str {
    if map.enabled && matches!(host, "localhost" | "127.0.0.1" | "0.0.0.0") {
        &map.mapped_host
    } else {
        host
    }
}

/// Parse a `host:port` target string. Returns `None` on malformed input.
pub fn parse_target(raw: &str) -> Option<(String, u16)> {
    let mut parts = raw.trim().split(':');
    let host = parts.next()?;
    let port = parts.next()?.parse::<u16>().ok()?;
    // Reject inputs with more than one colon, mirroring the old proxy.
    if host.is_empty() || parts.next().is_some() {
        return None;
    }
    Some((host.to_owned(), port))
}

/// Accept loop: serve connections until Ctrl+C, then shut down.
pub async fn run(listener: TcpListener, map: DockerMap) -> io::Result<()> {
    let mut connections = JoinSet::new();
    loop {
        tokio::select! {
            _ = signal::ctrl_c() => {
                info!("[Server] Shutting down...");
                break;
            }
            accepted = listener.accept() => {
                let (stream, peer) = match accepted {
                    Ok(conn) => conn,
                    Err(err) => {
                        warn!("[Server] Failed to accept connection: {err}");
                        continue;
                    }
                };
                info!("[WebSocket] Client connected from {peer}");
                let map = DockerMap {
                    enabled: map.enabled,
                    mapped_host: map.mapped_host.clone(),
                };
                connections.spawn(async move {
                    match tokio_tungstenite::accept_async(stream).await {
                        Ok(ws) => handle_connection(ws, &map).await,
                        Err(err) => warn!("[WebSocket] Handshake failed: {err}"),
                    }
                });
            }
        }
    }
    // Dropping the JoinSet aborts in-flight connections, matching the abrupt
    // shutdown behaviour of the previous implementation on Ctrl+C.
    drop(connections);
    Ok(())
}

/// Handle one proxied connection: read the target, dial it, then bridge.
pub async fn handle_connection(mut ws: WebSocketStream<TcpStream>, map: &DockerMap) {
    debug!("[WebSocket] Waiting for target address...");

    let target_msg = match ws.next().await {
        Some(Ok(Message::Text(text))) => text,
        Some(Ok(Message::Binary(bytes))) => match String::from_utf8(bytes) {
            Ok(text) => text,
            Err(_) => {
                warn!("[Error] First message is not valid UTF-8, expected 'host:port'");
                return;
            }
        },
        Some(Ok(other)) => {
            warn!("[Error] Expected target address, got {other:?}");
            return;
        }
        Some(Err(err)) => {
            warn!("[WebSocket] Error while waiting for target address: {err}");
            return;
        }
        None => return,
    };

    let (original_host, port) = match parse_target(&target_msg) {
        Some(target) => target,
        None => {
            warn!("[Error] Invalid target format '{target_msg}', expected 'host:port'");
            return;
        }
    };
    let host = original_host.clone();

    let host = remap_target_host(&host, map);
    if host != original_host.as_str() {
        info!("[Proxy] Redirecting {original_host} -> {host} (Docker localhost mapping)");
    }

    info!("[WebSocket] Client requested connection to {host}:{port}");
    info!("[TCP] Connecting to {host}:{port}");
    let mut tcp = match TcpStream::connect((host, port)).await {
        Ok(tcp) => tcp,
        Err(err) if err.kind() == io::ErrorKind::ConnectionRefused => {
            warn!("[TCP] Connection refused to {host}:{port}");
            warn!("[TCP] Is the target server running?");
            return;
        }
        Err(err) => {
            warn!("[TCP] Failed to connect to {host}:{port}: {err}");
            return;
        }
    };
    info!("[TCP] Connected to target server");

    let (ws_tx, ws_rx) = ws.split();
    {
        let (tcp_read, tcp_write) = tcp.split();
        let ws_to_tcp = forward_ws_to_tcp(ws_rx, tcp_write);
        let tcp_to_ws = forward_tcp_to_ws(tcp_read, ws_tx);

        // As soon as one direction finishes the connection is over; dropping
        // the other future cancels it.
        tokio::select! {
            _ = ws_to_tcp => {}
            _ = tcp_to_ws => {}
        }
    }

    // Do not rely on split-half drop timing here. In particular, a browser
    // close must synchronously tear down both directions of the Java socket so
    // the game server observes EOF and runs its disconnect/save path now.
    if let Err(err) = SockRef::from(&tcp).shutdown(Shutdown::Both) {
        warn!("[TCP] Failed to shut down target connection: {err}");
    }
}

/// Forward WebSocket data frames to the TCP target.
///
/// Only binary frames carry game packets; text frames after the handshake are
/// protocol violations and are dropped with a warning.
async fn forward_ws_to_tcp(
    mut ws_rx: futures_util::stream::SplitStream<WebSocketStream<TcpStream>>,
    mut tcp: tokio::net::tcp::WriteHalf<'_>,
) {
    while let Some(result) = ws_rx.next().await {
        match result {
            Ok(Message::Binary(bytes)) => {
                if let Err(err) = tcp.write_all(&bytes).await {
                    warn!("[WS→TCP] Error: {err}");
                    break;
                }
                debug!("[WS→TCP] Forwarded {} bytes", bytes.len());
            }
            Ok(Message::Text(_)) => {
                warn!("[WS→TCP] Warning: Received text message, expected binary");
            }
            Ok(Message::Close(_)) => {
                debug!("[WS→TCP] WebSocket connection closed");
                break;
            }
            // Ping/Pong are answered internally by the WebSocket layer.
            Ok(_) => {}
            Err(err) => {
                debug!("[WS→TCP] WebSocket connection closed: {err}");
                break;
            }
        }
    }
    let _ = tcp.shutdown().await;
}

/// Forward TCP data to the WebSocket as binary frames.
///
/// Each read becomes its own frame (see `TCP_READ_SIZE`) so frame boundaries
/// from the game server are preserved for the client.
async fn forward_tcp_to_ws(
    mut tcp: tokio::net::tcp::ReadHalf<'_>,
    mut ws_tx: futures_util::stream::SplitSink<WebSocketStream<TcpStream>, Message>,
) {
    let mut buffer = vec![0u8; TCP_READ_SIZE];
    loop {
        match tcp.read(&mut buffer).await {
            Ok(0) => {
                info!("[TCP→WS] TCP connection closed by server");
                break;
            }
            Ok(count) => {
                if ws_tx
                    .send(Message::Binary(buffer[..count].to_vec()))
                    .await
                    .is_err()
                {
                    debug!("[TCP→WS] WebSocket connection closed");
                    return;
                }
                debug!("[TCP→WS] Forwarded {count} bytes");
            }
            Err(err) => {
                warn!("[TCP→WS] Error: {err}");
                break;
            }
        }
    }
    // Politely close the browser side when the game server hangs up.
    let _ = ws_tx.send(Message::Close(None)).await;
}

#[cfg(test)]
mod tests {
    use super::*;

    fn map(enabled: bool) -> DockerMap {
        DockerMap {
            enabled,
            mapped_host: "host.docker.internal".to_owned(),
        }
    }

    #[test]
    fn parse_target_accepts_host_port() {
        assert_eq!(
            parse_target("127.0.0.1:8484"),
            Some(("127.0.0.1".to_owned(), 8484))
        );
        assert_eq!(
            parse_target("  example.com:99  "),
            Some(("example.com".to_owned(), 99))
        );
    }

    #[test]
    fn parse_target_rejects_malformed_input() {
        assert_eq!(parse_target("no-port"), None);
        assert_eq!(parse_target("host:notaport"), None);
        assert_eq!(parse_target("a:b:c"), None);
        assert_eq!(parse_target(":8484"), None);
        assert_eq!(parse_target(""), None);
        assert_eq!(parse_target("host:99999"), None);
    }

    #[test]
    fn remap_only_touches_loopback_when_enabled() {
        let on = map(true);
        let off = map(false);
        assert_eq!(remap_target_host("localhost", &on), "host.docker.internal");
        assert_eq!(remap_target_host("127.0.0.1", &on), "host.docker.internal");
        assert_eq!(remap_target_host("0.0.0.0", &on), "host.docker.internal");
        assert_eq!(remap_target_host("example.com", &on), "example.com");
        assert_eq!(remap_target_host("localhost", &off), "localhost");
    }
}
