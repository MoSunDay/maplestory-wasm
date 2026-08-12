//! ws-proxy entry point: parse CLI arguments and serve the proxy.

use clap::Parser;
use tracing::info;
use tracing_subscriber::EnvFilter;

#[derive(Parser)]
#[command(
    version,
    about = "WebSocket-to-TCP proxy for the MapleStory WASM client"
)]
struct Cli {
    /// WebSocket port to listen on
    #[arg(long, default_value_t = 8080)]
    ws_port: u16,

    /// Address to bind the WebSocket listener to
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
            EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info")),
        )
        .init();

    let cli = Cli::parse();
    let listener = tokio::net::TcpListener::bind((cli.bind.as_str(), cli.ws_port)).await?;
    let local = listener.local_addr()?;

    info!("============================================================");
    info!("Generic WebSocket-to-TCP Proxy");
    info!("============================================================");
    info!("WebSocket server: ws://{local}");
    info!("Clients will specify target TCP server on connection");
    info!("============================================================");
    info!("Waiting for connections...");

    ws_proxy::run(listener, ws_proxy::DockerMap::from_env()).await
}
