//! web-server entry point: serve the client bundle and assets over HTTP.

use std::path::PathBuf;

use clap::Parser;
use tokio::net::TcpListener;
use tokio::signal;
use tracing::{info, warn};

mod respond;
mod serve;

#[derive(Parser)]
#[command(
    version,
    about = "HTTP static file server for the MapleStory WASM client"
)]
struct Cli {
    /// Port to listen on
    #[arg(long, default_value_t = 8000)]
    port: u16,

    /// Directory to serve (the repository root in a normal deployment)
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
            format!(
                "cannot resolve directory '{}': {err}",
                cli.directory.display()
            ),
        )
    })?;

    let listener = TcpListener::bind((cli.bind.as_str(), cli.port)).await?;
    info!("============================================================");
    info!("MapleStory WASM Server with HTTP Range Request Support");
    info!("============================================================");
    info!("Serving at http://localhost:{}", cli.port);
    info!("Directory: {}", root.display());
    info!("Features:");
    info!("  - HTTP Range requests (206 Partial Content)");
    info!("  - CORS headers for cross-origin access");
    info!("  - COOP/COEP headers for SharedArrayBuffer");
    info!("  - LazyFS asset loading support");
    info!("Press Ctrl+C to stop the server.");
    info!("============================================================");
    info!("Listening on http://{}", listener.local_addr()?);

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
                let root = root.clone();
                tokio::spawn(async move {
                    serve::handle_connection(stream, peer, &root).await;
                });
            }
        }
    }
    Ok(())
}
