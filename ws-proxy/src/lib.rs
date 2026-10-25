//! WebSocket-to-TCP proxy for the MapleStory WASM client.
//!
//! Browsers cannot open raw TCP sockets, so the WASM client opens a
//! WebSocket to this proxy instead. The first WebSocket message (text or
//! binary) carries the target address as `host:port`; everything after that
//! is forwarded verbatim between the WebSocket and a TCP connection to the
//! game server.

use std::io;
use std::net::Shutdown;
use std::time::Duration;

use futures_util::{SinkExt, StreamExt};
use socket2::SockRef;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::signal;
use tokio::task::JoinSet;
use tokio::time::{interval_at, Instant, MissedTickBehavior};
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::WebSocketStream;
use tracing::{debug, info, warn};

/// Maximum bytes read from TCP per iteration. Each read becomes exactly one
/// binary WebSocket frame, which matters right after connecting: the game
/// server sends a 16-byte Hello packet and the client expects its first
/// WebSocket message to contain exactly those 16 bytes.
pub const TCP_READ_SIZE: usize = 64 * 1024;

/// Default delay between WebSocket pings sent to the browser.
pub const DEFAULT_PING_INTERVAL_SECS: u64 = 30;

/// Default grace period without any downstream frame before the session is
/// presumed dead (browser killed without a close frame).
pub const DEFAULT_LIVENESS_TIMEOUT_SECS: u64 = 90;

/// Environment variable overriding `DEFAULT_PING_INTERVAL_SECS`.
pub const PING_INTERVAL_ENV: &str = "WS_PROXY_PING_INTERVAL_SECS";

/// Environment variable overriding `DEFAULT_LIVENESS_TIMEOUT_SECS`.
pub const LIVENESS_TIMEOUT_ENV: &str = "WS_PROXY_LIVENESS_TIMEOUT_SECS";

/// Downstream liveness configuration for one proxied session.
///
/// A killed browser never sends a WebSocket close frame, so the proxy cannot
/// rely on `ws_rx` ending by itself. Instead it pings the browser every
/// `ping_interval` and refreshes `last_alive` on every received frame (pong,
/// binary, text, ping). If nothing arrives for `liveness_timeout`, the
/// session is dropped and the upstream TCP connection is torn down so the
/// game server immediately runs its disconnect path instead of letting the
/// account stay `loggedin` for minutes.
#[derive(Clone, Copy, Debug)]
pub struct LivenessConfig {
    pub ping_interval: Duration,
    pub liveness_timeout: Duration,
}

impl LivenessConfig {
    /// Production defaults: ping every 30s, give up after 90s of silence
    /// (three missed ping round-trips).
    pub fn defaults() -> Self {
        Self {
            ping_interval: Duration::from_secs(DEFAULT_PING_INTERVAL_SECS),
            liveness_timeout: Duration::from_secs(DEFAULT_LIVENESS_TIMEOUT_SECS),
        }
    }

    /// Build from an explicit pair of second values, as used by tests.
    pub fn from_secs(ping_interval: u64, liveness_timeout: u64) -> Self {
        Self {
            ping_interval: Duration::from_secs(ping_interval),
            liveness_timeout: Duration::from_secs(liveness_timeout),
        }
    }

    /// Read overrides from the environment, falling back to the defaults on
    /// missing or malformed values. Only the binary entry point uses this;
    /// everything else takes the config as an explicit parameter.
    pub fn from_env() -> Self {
        Self {
            ping_interval: Duration::from_secs(env_secs(
                PING_INTERVAL_ENV,
                DEFAULT_PING_INTERVAL_SECS,
            )),
            liveness_timeout: Duration::from_secs(env_secs(
                LIVENESS_TIMEOUT_ENV,
                DEFAULT_LIVENESS_TIMEOUT_SECS,
            )),
        }
    }
}

/// Parse a positive seconds value from the environment, falling back to
/// `default` when unset, unparsable, or zero (a zero interval would spin).
fn env_secs(name: &str, default: u64) -> u64 {
    std::env::var(name)
        .ok()
        .and_then(|value| value.trim().parse::<u64>().ok())
        .filter(|secs| *secs > 0)
        .unwrap_or(default)
}

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
pub async fn run(
    listener: TcpListener,
    map: DockerMap,
    liveness: LivenessConfig,
) -> io::Result<()> {
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
                        Ok(ws) => handle_connection(ws, &map, liveness).await,
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
///
/// `liveness` controls downstream keep-alive (see [`LivenessConfig`]).
pub async fn handle_connection(
    mut ws: WebSocketStream<TcpStream>,
    map: &DockerMap,
    liveness: LivenessConfig,
) {
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
        bridge_session(ws_rx, ws_tx, tcp_read, tcp_write, liveness).await;
    }

    // Do not rely on split-half drop timing here. In particular, a browser
    // close must synchronously tear down both directions of the Java socket so
    // the game server observes EOF and runs its disconnect/save path now.
    if let Err(err) = SockRef::from(&tcp).shutdown(Shutdown::Both) {
        warn!("[TCP] Failed to shut down target connection: {err}");
    }
}

/// Bridge the WebSocket and the TCP target in a single `select!` loop with
/// three arms: downstream frames, upstream bytes, and a liveness tick.
///
/// - Downstream (`ws_rx.next()`): binary frames are forwarded to the TCP
///   target, text frames are dropped (protocol violation), `Close` ends the
///   session. Ping/Pong replies are handled automatically by the WebSocket
///   layer, but *any* received frame refreshes the liveness deadline.
/// - Upstream (`tcp_rx.read()`): bytes are forwarded to the browser as one
///   binary frame per read; EOF closes the browser side.
/// - Tick: send `Message::Ping` every `ping_interval` and abandon sessions
///   that have been silent for longer than `liveness_timeout`. This is what
///   reclaims sessions left behind by a killed browser that never sends a
///   close frame (otherwise `ws_rx.next()` would pend forever and the
///   account would stay `loggedin` on the game server).
///
/// The deadline starts at connection establishment, so a browser that never
/// sends a single frame after the handshake is reclaimed as well.
async fn bridge_session(
    mut ws_rx: futures_util::stream::SplitStream<WebSocketStream<TcpStream>>,
    mut ws_tx: futures_util::stream::SplitSink<WebSocketStream<TcpStream>, Message>,
    mut tcp_rx: tokio::net::tcp::ReadHalf<'_>,
    mut tcp_tx: tokio::net::tcp::WriteHalf<'_>,
    liveness: LivenessConfig,
) {
    let mut last_alive = Instant::now();
    // First ping one full interval after the handshake (a just-connected
    // browser is alive by definition), then every `ping_interval`.
    let mut ping_tick = interval_at(
        Instant::now() + liveness.ping_interval,
        liveness.ping_interval,
    );
    // Never fire a burst of ticks after a stall; one check per period is
    // enough, and a burst would only spam a wedged connection.
    ping_tick.set_missed_tick_behavior(MissedTickBehavior::Delay);
    let mut buffer = vec![0u8; TCP_READ_SIZE];

    loop {
        tokio::select! {
            downstream = ws_rx.next() => {
                match downstream {
                    Some(Ok(Message::Binary(bytes))) => {
                        last_alive = Instant::now();
                        if let Err(err) = tcp_tx.write_all(&bytes).await {
                            warn!("[WS→TCP] Error: {err}");
                            break;
                        }
                        debug!("[WS→TCP] Forwarded {} bytes", bytes.len());
                    }
                    Some(Ok(Message::Text(_))) => {
                        last_alive = Instant::now();
                        warn!("[WS→TCP] Warning: Received text message, expected binary");
                    }
                    Some(Ok(Message::Close(_))) => {
                        debug!("[WS→TCP] WebSocket connection closed");
                        break;
                    }
                    // Ping/Pong (and anything else) is answered internally by
                    // the WebSocket layer; receiving it still proves that the
                    // browser is alive.
                    Some(Ok(_)) => {
                        last_alive = Instant::now();
                    }
                    Some(Err(err)) => {
                        debug!("[WS→TCP] WebSocket connection closed: {err}");
                        break;
                    }
                    None => {
                        debug!("[WS→TCP] WebSocket stream ended");
                        break;
                    }
                }
            }
            read = tcp_rx.read(&mut buffer) => {
                match read {
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
            _ = ping_tick.tick() => {
                if last_alive.elapsed() > liveness.liveness_timeout {
                    // The browser is gone; a close frame would be written
                    // into a black hole and waiting on it would only delay
                    // the upstream teardown, so return immediately.
                    // `handle_connection` then hard-shuts the target socket.
                    warn!(
                        "[Liveness] No frame from browser for {}s (timeout {}s), dropping session",
                        last_alive.elapsed().as_secs(),
                        liveness.liveness_timeout.as_secs()
                    );
                    return;
                }
                if ws_tx.send(Message::Ping(Vec::new())).await.is_err() {
                    debug!("[Liveness] WebSocket connection closed while pinging");
                    return;
                }
                debug!("[Liveness] Pinged browser");
            }
        }
    }

    // Session over (close frame, stream end, or upstream EOF): politely
    // close the browser side and stop feeding the target. The synchronous
    // target shutdown happens in `handle_connection`.
    let _ = ws_tx.send(Message::Close(None)).await;
    let _ = tcp_tx.shutdown().await;
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

    #[test]
    fn liveness_defaults_are_30s_ping_and_90s_timeout() {
        let config = LivenessConfig::defaults();
        assert_eq!(config.ping_interval, Duration::from_secs(30));
        assert_eq!(config.liveness_timeout, Duration::from_secs(90));
        assert_eq!(
            LivenessConfig::from_secs(1, 2).ping_interval,
            Duration::from_secs(1)
        );
        assert_eq!(
            LivenessConfig::from_secs(1, 2).liveness_timeout,
            Duration::from_secs(2)
        );
    }

    #[test]
    fn liveness_default_timeout_tolerates_three_missed_pings() {
        // Guard against future default drift: the timeout must absorb at
        // least three full ping intervals of silence.
        let config = LivenessConfig::defaults();
        assert!(config.liveness_timeout >= config.ping_interval * 3);
    }

    #[test]
    fn env_secs_rejects_garbage_and_zero() {
        // Directly exercise the parsing rules used by `LivenessConfig::from_env`.
        assert_eq!(env_secs_value("12"), Some(12));
        assert_eq!(env_secs_value(" 7 "), Some(7));
        assert_eq!(env_secs_value("abc"), None);
        assert_eq!(env_secs_value("0"), None);
        assert_eq!(env_secs_value("-3"), None);
        assert_eq!(env_secs_value(""), None);
    }

    fn env_secs_value(raw: &str) -> Option<u64> {
        raw.trim().parse::<u64>().ok().filter(|secs| *secs > 0)
    }
}
