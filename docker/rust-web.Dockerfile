# Multi-stage build for the Rust web stack:
#   web-server     - static file server for the client page
#   ws-proxy       - WebSocket -> TCP bridge for the game connection
#   assets-server  - on-demand game data (LazyFS) server
FROM rust:slim-bookworm AS builder
WORKDIR /build

# Copy manifests and crate sources; assets are not needed at build time.
COPY Cargo.toml Cargo.lock ./
COPY ws-proxy ./ws-proxy
COPY assets-server ./assets-server
COPY web-server ./web-server

RUN cargo build --release --workspace

FROM debian:bookworm-slim
WORKDIR /app

COPY --from=builder /build/target/release/web-server /usr/local/bin/web-server
COPY --from=builder /build/target/release/ws-proxy /usr/local/bin/ws-proxy
COPY --from=builder /build/target/release/assets-server /usr/local/bin/assets-server
