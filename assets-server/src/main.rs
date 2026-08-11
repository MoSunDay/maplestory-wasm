//! assets-server entry point: WebSocket server streaming NX chunks to LazyFS.

use std::net::SocketAddr;
use std::path::PathBuf;

use clap::Parser;
use futures_util::{SinkExt, StreamExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::signal;
use tokio_tungstenite::tungstenite::protocol::{Message, WebSocketConfig};
use tracing::{info, warn};

mod serve;

#[derive(Parser)]
#[command(version, about = "WebSocket asset server for the MapleStory WASM client")]
struct Cli {
    /// Port to listen on
    #[arg(long, default_value_t = 8765)]
    port: u16,

    /// Directory to serve files from (asset subdirectories are probed)
    #[arg(long, default_value = ".")]
    directory: PathBuf,

    /// Address to bind the listener to
    #[arg(long, default_value = "0.0.0.0")]
    bind: String,
}

#[tokio::main]
async fn main() -> std::io::Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_target(false)
        .without_time()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let cli = Cli::parse();
    let root = std::fs::canonicalize(&cli.directory).map_err(|err| {
        std::io::Error::new(
            err.kind(),
            format!("cannot resolve directory '{}': {err}", cli.directory.display()),
        )
    })?;
    info!("[AssetServer] Serving files from: {}", root.display());
    info!("[AssetServer] Starting WebSocket server on port {}", cli.port);
    info!("[AssetServer] Connect with: ws://localhost:{}", cli.port);

    let listener = TcpListener::bind((cli.bind.as_str(), cli.port)).await?;
    info!("[AssetServer] Listening on ws://{}", listener.local_addr()?);
    loop {
        tokio::select! {
            _ = signal::ctrl_c() => {
                info!("[AssetServer] Shutting down...");
                break;
            }
            accepted = listener.accept() => {
                let (stream, peer) = match accepted {
                    Ok(conn) => conn,
                    Err(err) => {
                        warn!("[AssetServer] Failed to accept connection: {err}");
                        continue;
                    }
                };
                let root = root.clone();
                tokio::spawn(async move {
                    match tokio_tungstenite::accept_async_with_config(stream, Some(ws_config())).await {
                        Ok(ws) => handle_connection(&root, peer, ws).await,
                        Err(err) => warn!("[AssetServer] Handshake failed: {err}"),
                    }
                });
            }
        }
    }
    Ok(())
}

/// Mirror the old server: generous message limit, no compression.
fn ws_config() -> WebSocketConfig {
    WebSocketConfig {
        max_message_size: Some(50 * 1024 * 1024),
        ..WebSocketConfig::default()
    }
}

async fn handle_connection(
    root: &std::path::Path,
    peer: SocketAddr,
    ws: tokio_tungstenite::WebSocketStream<TcpStream>,
) {
    info!("[AssetServer] Client connected: {peer}");
    let (mut tx, mut rx) = ws.split();

    while let Some(result) = rx.next().await {
        let raw = match result {
            Ok(Message::Text(text)) => text,
            Ok(Message::Binary(bytes)) => match String::from_utf8(bytes) {
                Ok(text) => text,
                Err(_) => {
                    let _ = send_error(&mut tx, "Messages must be UTF-8 JSON").await;
                    continue;
                }
            },
            Ok(Message::Close(_)) => break,
            // Ping/Pong are answered internally by the WebSocket layer.
            Ok(_) => continue,
            Err(err) => {
                warn!("[AssetServer] Connection error from {peer}: {err}");
                break;
            }
        };

        for frame in serve::process_message(root, &raw).await {
            let message = match frame {
                serve::Frame::Text(text) => Message::Text(text),
                serve::Frame::Binary(bytes) => Message::Binary(bytes),
            };
            if let Err(err) = tx.send(message).await {
                warn!("[AssetServer] Failed to send to {peer}: {err}");
                info!("[AssetServer] Client disconnected: {peer}");
                return;
            }
        }
    }
    info!("[AssetServer] Client disconnected: {peer}");
}

async fn send_error(
    tx: &mut futures_util::stream::SplitSink<
        tokio_tungstenite::WebSocketStream<TcpStream>,
        Message,
    >,
    message: &str,
) -> Result<(), tokio_tungstenite::tungstenite::Error> {
    tx.send(Message::Text(
        serde_json::json!({ "type": "error", "message": message }).to_string(),
    ))
    .await
}
