//! LazyFS asset protocol implementation.
//!
//! Wire protocol (must stay compatible with `src/client/LazyFS/lazyfs.js`):
//!
//!   client -> {"type": "get_size", "file": "UI.nx"}
//!   server -> {"type": "size", "file": "UI.nx", "size": N, "version": V}
//!
//!   client -> {"type": "get_chunks", "file": "UI.nx", "start": 0, "end": 10,
//!              "chunk_size": 524288}
//!   server -> binary frames: [u32 LE chunk index][u8 filename len][filename][data]
//!   server -> {"type": "chunks_done", "file": "UI.nx", "start": 0, "end": 10}
//!
//!   client -> {"type": "get_chunk", "file": "UI.nx", "index": 3, "chunk_size": N}
//!   server -> one binary frame, or {"type": "error", ...} when missing

use serde_json::{json, Value};
use tracing::warn;

use crate::cache::AssetCache;

const DEFAULT_CHUNK_SIZE: u64 = 512 * 1024;
/// Safety valve for absurd batch requests; real clients ask for small batches.
const MAX_BATCH_SPAN: u64 = 100_000;

/// One outgoing WebSocket frame.
pub enum Frame {
    Text(String),
    Binary(Vec<u8>),
}

/// Encode a chunk into the binary frame layout expected by lazyfs.js.
/// Returns `None` when the filename is too long for the 1-byte length field.
pub fn encode_chunk_frame(index: u64, filename: &str, data: &[u8]) -> Option<Vec<u8>> {
    let name_bytes = filename.as_bytes();
    if index > u32::MAX as u64 || name_bytes.len() > usize::from(u8::MAX) {
        return None;
    }
    let mut frame = Vec::with_capacity(5 + name_bytes.len() + data.len());
    frame.extend_from_slice(&(index as u32).to_le_bytes());
    frame.push(name_bytes.len() as u8);
    frame.extend_from_slice(name_bytes);
    frame.extend_from_slice(data);
    Some(frame)
}

fn error_frame(message: impl Into<String>) -> Frame {
    Frame::Text(json!({ "type": "error", "message": message.into() }).to_string())
}

fn get_field<'a>(data: &'a Value, key: &str) -> Option<&'a Value> {
    data.get(key).filter(|value| !value.is_null())
}

fn get_u64(data: &Value, key: &str, default: u64) -> Result<u64, String> {
    match get_field(data, key) {
        None => Ok(default),
        Some(value) => value
            .as_u64()
            .ok_or_else(|| format!("Field '{key}' must be a non-negative integer")),
    }
}

/// Handle one incoming message, producing all response frames.
pub async fn process_message(cache: &AssetCache, raw: &str) -> Vec<Frame> {
    let data: Value = match serde_json::from_str(raw) {
        Ok(value) => value,
        Err(_) => return vec![error_frame("Invalid JSON")],
    };

    let msg_type = data.get("type").and_then(Value::as_str);
    match msg_type {
        Some("get_size") => handle_get_size(cache, &data),
        Some("get_chunks") => handle_get_chunks(cache, &data).await,
        Some("get_chunk") => handle_get_chunk(cache, &data).await,
        Some(other) => vec![error_frame(format!("Unknown message type: {other}"))],
        None => vec![error_frame("Unknown message type: null")],
    }
}

fn handle_get_size(cache: &AssetCache, data: &Value) -> Vec<Frame> {
    let Some(filename) = get_field(data, "file").and_then(Value::as_str) else {
        return vec![error_frame("Missing 'file' field")];
    };
    let (size, version) = match cache.get(filename) {
        Some(asset) => (asset.size() as i64, asset.version()),
        None => {
            warn!("[AssetServer] File not found in NX asset index: {filename}");
            (-1, -1)
        }
    };
    vec![Frame::Text(
        json!({ "type": "size", "file": filename, "size": size, "version": version }).to_string(),
    )]
}

async fn handle_get_chunks(cache: &AssetCache, data: &Value) -> Vec<Frame> {
    let Some(filename) = get_field(data, "file").and_then(Value::as_str) else {
        return vec![error_frame("Missing 'file' field")];
    };
    let (start, end, chunk_size) = match batch_params(data) {
        Ok(params) => params,
        Err(message) => return vec![error_frame(message)],
    };

    let Some(asset) = cache.get(filename) else {
        return vec![error_frame(format!(
            "File not found in NX asset index: {filename}"
        ))];
    };
    let mut frames = Vec::new();
    for index in start..=end {
        let data = match asset.read_chunk(index, chunk_size).await {
            Ok(data) => data,
            Err(err) => return vec![error_frame(err.to_string())],
        };
        if !data.is_empty() {
            match encode_chunk_frame(index, filename, &data) {
                Some(frame) => frames.push(Frame::Binary(frame)),
                None => {
                    return vec![error_frame(format!(
                        "Cannot encode chunk {filename}:{index}"
                    ))]
                }
            }
        }
    }

    frames.push(Frame::Text(
        json!({ "type": "chunks_done", "file": filename, "start": start, "end": end }).to_string(),
    ));
    frames
}

async fn handle_get_chunk(cache: &AssetCache, data: &Value) -> Vec<Frame> {
    let Some(filename) = get_field(data, "file").and_then(Value::as_str) else {
        return vec![error_frame("Missing 'file' field")];
    };
    let index = match get_u64(data, "index", 0) {
        Ok(value) => value,
        Err(message) => return vec![error_frame(message)],
    };
    let chunk_size = match get_u64(data, "chunk_size", DEFAULT_CHUNK_SIZE) {
        Ok(value) => value,
        Err(message) => return vec![error_frame(message)],
    };

    let Some(asset) = cache.get(filename) else {
        return vec![error_frame(format!("Chunk not found: {filename}:{index}"))];
    };
    match asset.read_chunk(index, chunk_size).await {
        Ok(data) if !data.is_empty() => match encode_chunk_frame(index, filename, &data) {
            Some(frame) => vec![Frame::Binary(frame)],
            None => vec![error_frame(format!(
                "Cannot encode chunk {filename}:{index}"
            ))],
        },
        Ok(_) => vec![error_frame(format!("Chunk not found: {filename}:{index}"))],
        Err(err) => vec![error_frame(err.to_string())],
    }
}

fn batch_params(data: &Value) -> Result<(u64, u64, u64), String> {
    let start = get_u64(data, "start", 0)?;
    let end = get_u64(data, "end", start)?;
    let chunk_size = get_u64(data, "chunk_size", DEFAULT_CHUNK_SIZE)?;
    if end < start {
        return Err(format!("Invalid chunk range {start}..={end}"));
    }
    if end - start >= MAX_BATCH_SPAN {
        return Err(format!("Chunk range {start}..={end} is too large"));
    }
    Ok((start, end, chunk_size))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::Path;

    fn temp_root() -> tempfile::TempDir {
        tempfile::tempdir().unwrap()
    }

    async fn process(root: &Path, raw: &str) -> Vec<Frame> {
        let cache = AssetCache::load(root, false).unwrap();
        process_message(&cache, raw).await
    }

    #[test]
    fn encode_frame_layout() {
        let frame = encode_chunk_frame(7, "a.nx", &[1, 2, 3]).unwrap();
        assert_eq!(&frame[..4], &7u32.to_le_bytes());
        assert_eq!(frame[4], 4);
        assert_eq!(&frame[5..9], b"a.nx");
        assert_eq!(&frame[9..], &[1, 2, 3]);
        assert!(encode_chunk_frame(7, &"x".repeat(300), &[]).is_none());
        assert!(encode_chunk_frame(u32::MAX as u64 + 1, "a", &[]).is_none());
    }

    #[tokio::test]
    async fn process_get_size_and_chunks() {
        let root = temp_root();
        let contents: Vec<u8> = (0..100).collect();
        fs::write(root.path().join("test.nx"), &contents).unwrap();

        let frames = process(root.path(), r#"{"type":"get_size","file":"test.nx"}"#).await;
        assert_eq!(frames.len(), 1);
        let Frame::Text(text) = &frames[0] else {
            panic!()
        };
        let value: Value = serde_json::from_str(text).unwrap();
        assert_eq!(value["type"], "size");
        assert_eq!(value["size"], 100);
        assert!(value["version"].as_i64().unwrap() > 0);

        let frames = process(
            root.path(),
            r#"{"type":"get_chunks","file":"test.nx","start":0,"end":1,"chunk_size":60}"#,
        )
        .await;
        // Two chunk frames plus chunks_done.
        assert_eq!(frames.len(), 3);
        let Frame::Binary(first) = &frames[0] else {
            panic!()
        };
        assert_eq!(&first[5 + 7..], &contents[..60]);
        let Frame::Binary(second) = &frames[1] else {
            panic!()
        };
        assert_eq!(&second[5 + 7..], &contents[60..]);
        let Frame::Text(done) = &frames[2] else {
            panic!()
        };
        assert!(done.contains("chunks_done"));
    }

    #[tokio::test]
    async fn process_errors() {
        let root = temp_root();
        fs::write(root.path().join("placeholder.nx"), b"placeholder").unwrap();
        let frames = process(root.path(), "not json").await;
        let Frame::Text(text) = &frames[0] else {
            panic!()
        };
        assert!(text.contains("Invalid JSON"));

        let frames = process(root.path(), r#"{"type":"nope"}"#).await;
        let Frame::Text(text) = &frames[0] else {
            panic!()
        };
        assert!(text.contains("Unknown message type: nope"));

        let frames = process(
            root.path(),
            r#"{"type":"get_chunk","file":"missing.nx","index":0}"#,
        )
        .await;
        let Frame::Text(text) = &frames[0] else {
            panic!()
        };
        assert!(text.contains("Chunk not found"));

        let frames = process(root.path(), r#"{"type":"get_size","file":"missing.nx"}"#).await;
        let Frame::Text(text) = &frames[0] else {
            panic!()
        };
        let value: Value = serde_json::from_str(text).unwrap();
        assert_eq!(value["size"], -1);
        assert_eq!(value["version"], -1);
    }
}
