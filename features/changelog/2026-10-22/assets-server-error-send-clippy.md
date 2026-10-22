# assets-server: 收敛错误帧发送返回值

## Context

workspace Clippy 门禁开启 `-D warnings` 后，`send_error` 返回的
`tungstenite::Error` 触发 `result_large_err`。该函数只有一个调用点，
且调用点原本丢弃返回值，错误帧发送失败既不会被传播也不会被记录。

## Change Summary

- `assets-server/src/main.rs` 的 `send_error` 改为返回 `()`。
- 发送错误帧失败时通过现有 `warn!` 日志记录，调用点不再丢弃 `Result`。

## Impact Surface

- assets-server WebSocket 错误帧发送的失败观测。
- 不改变请求/响应协议和成功路径行为。

## Validation

- `cargo test --workspace --all-features --all-targets`
- `cargo clippy --workspace --all-targets --all-features -- -D warnings`
- `cargo build --workspace --all-targets --all-features`

三项均通过。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
