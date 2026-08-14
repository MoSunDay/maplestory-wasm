# Web Services for the MapleStory WASM Client

The browser cannot open raw TCP connections, and the game data files are too
large to download up front. The Rust services in this repository's workspace
solve both problems:

| Crate | Default port | Role |
|---|---|---|
| `web-server` | 8000 | Serves the client (`index.html`, `.js`, `.wasm`, `.css`) with the COOP/COEP headers required for `SharedArrayBuffer`, supports HTTP Range requests |
| `ws-proxy` | 8080 | Bridges the client's WebSocket game connection to the TCP port of a regular MapleStory server |
| `assets-server` | 8765 | Streams on-demand game data (LazyFS protocol) so only the chunks the client actually reads are transferred |

## Running locally

Build once from the repository root:

```bash
cargo build --release -p web-server -p ws-proxy -p assets-server
```

Then start the three services in separate terminals:

```bash
./target/release/web-server --port 8000 --directory .
./target/release/ws-proxy --ws-port 8080
./target/release/assets-server --port 8765 --directory .
```

All binaries accept `--bind` (default `::`, dual-stack on supported systems); pass `--port 0`
(`--ws-port 0` for `ws-proxy`) for a random free port. Open `http://localhost:8000` once they are running.

The game connection works like this:

```
Browser (WASM client) <--WebSocket--> ws-proxy <--TCP--> MapleStory server
```

The client sends the target as the first WebSocket message (`host:port`, e.g.
`127.0.0.1:8484`); the proxy connects to the linked server there and forwards bytes in both
directions. Set `WS_PROXY_LOCALHOST_TARGET=<host>` to remap `127.0.0.1` /
`localhost` targets (needed when the proxy runs in Docker and the game server
on the host is reachable via `host.docker.internal`).

`assets-server` speaks the LazyFS protocol used by `src/client/LazyFS`:
`get_size` / `get_chunks` / `get_chunk` requests answered with binary chunk
frames, which the client reassembles into a virtual filesystem.

By default the asset server reads requested chunks from disk. Enable its full
NX memory snapshot when the host has enough RAM:

```bash
# Docker stack
ASSETS_CACHE_ALL_NX=true ./scripts/run_all.sh

# Local binary
./target/release/assets-server --port 8765 --directory . --cache-all-nx
```

The switch is off by default. In memory mode every `.nx` found under the
served root and its `assets`, `serverAssets`, `wz`, and `data` directories is
loaded before the port starts listening. Restart the service after replacing
NX files.
