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

use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

use serde_json::{json, Value};
use tracing::warn;

const DEFAULT_CHUNK_SIZE: u64 = 512 * 1024;
/// Safety valve for absurd batch requests; real clients ask for small batches.
const MAX_BATCH_SPAN: u64 = 100_000;

/// One outgoing WebSocket frame.
pub enum Frame {
    Text(String),
    Binary(Vec<u8>),
}

/// Resolve a requested filename to a concrete path under `root`.
///
/// Only the basename is used (directory components from the client are
/// ignored, preventing traversal), and common asset subdirectories are
/// probed in order.
pub fn resolve_file(root: &Path, filename: &str) -> PathBuf {
    let safe = match Path::new(filename).file_name() {
        Some(name) if name != ".." => name.to_owned(),
        // Unresolvable name: point at a path that never exists.
        _ => return root.join("__no_such_asset__"),
    };

    for subdir in ["", "assets", "serverAssets", "wz", "data"] {
        let candidate = if subdir.is_empty() {
            root.join(&safe)
        } else {
            root.join(subdir).join(&safe)
        };
        if candidate.exists() {
            return candidate;
        }
    }
    root.join(safe)
}

/// File size and a stable version token (mtime in nanoseconds); -1 if missing.
pub fn file_size_and_version(path: &Path) -> (i64, i64) {
    match std::fs::metadata(path) {
        Ok(meta) if meta.is_file() => {
            let version = meta
                .modified()
                .ok()
                .and_then(|mtime| mtime.duration_since(UNIX_EPOCH).ok())
                .map(|elapsed| elapsed.as_nanos() as i64)
                .unwrap_or(-1);
            (meta.len() as i64, version)
        }
        _ => (-1, -1),
    }
}

/// Read one chunk of `chunk_size` bytes starting at `index * chunk_size`.
/// Returns an empty vector when the chunk lies past the end of the file.
pub async fn read_chunk(path: &Path, index: u64, chunk_size: u64) -> std::io::Result<Vec<u8>> {
    if chunk_size == 0 || !path.exists() {
        return Ok(Vec::new());
    }
    let start = index.saturating_mul(chunk_size);
    let path = path.to_owned();
    tokio::task::spawn_blocking(move || {
        let mut file = std::fs::File::open(&path)?;
        let file_size = file.metadata()?.len();
        if start >= file_size {
            return Ok(Vec::new());
        }
        let count = (file_size - start).min(chunk_size);
        file.seek(SeekFrom::Start(start))?;
        let mut buffer = Vec::with_capacity(count as usize);
        file.take(count).read_to_end(&mut buffer)?;
        Ok(buffer)
    })
    .await
    .expect("chunk reader task panicked")
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
pub async fn process_message(root: &Path, raw: &str) -> Vec<Frame> {
    let data: Value = match serde_json::from_str(raw) {
        Ok(value) => value,
        Err(_) => return vec![error_frame("Invalid JSON")],
    };

    let msg_type = data.get("type").and_then(Value::as_str);
    match msg_type {
        Some("get_size") => handle_get_size(root, &data).await,
        Some("get_chunks") => handle_get_chunks(root, &data).await,
        Some("get_chunk") => handle_get_chunk(root, &data).await,
        Some(other) => vec![error_frame(format!("Unknown message type: {other}"))],
        None => vec![error_frame("Unknown message type: null")],
    }
}

async fn handle_get_size(root: &Path, data: &Value) -> Vec<Frame> {
    let Some(filename) = get_field(data, "file").and_then(Value::as_str) else {
        return vec![error_frame("Missing 'file' field")];
    };
    let path = resolve_file(root, filename);
    if !path.exists() {
        warn!("[AssetServer] File not found: {filename} (checked {} and subdirs)", path.display());
    }
    let (size, version) = file_size_and_version(&path);
    vec![Frame::Text(
        json!({ "type": "size", "file": filename, "size": size, "version": version })
            .to_string(),
    )]
}

async fn handle_get_chunks(root: &Path, data: &Value) -> Vec<Frame> {
    let Some(filename) = get_field(data, "file").and_then(Value::as_str) else {
        return vec![error_frame("Missing 'file' field")];
    };
    let (start, end, chunk_size) = match batch_params(data) {
        Ok(params) => params,
        Err(message) => return vec![error_frame(message)],
    };

    let path = resolve_file(root, filename);
    let mut frames = Vec::new();
    for index in start..=end {
        match read_chunk(&path, index, chunk_size).await {
            Ok(data) if data.is_empty() => {}
            Ok(data) => match encode_chunk_frame(index, filename, &data) {
                Some(frame) => frames.push(Frame::Binary(frame)),
                None => return vec![error_frame(format!("Cannot encode chunk {filename}:{index}"))],
            },
            Err(err) => return vec![error_frame(err.to_string())],
        }
    }

    frames.push(Frame::Text(
        json!({ "type": "chunks_done", "file": filename, "start": start, "end": end })
            .to_string(),
    ));
    frames
}

async fn handle_get_chunk(root: &Path, data: &Value) -> Vec<Frame> {
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

    let path = resolve_file(root, filename);
    match read_chunk(&path, index, chunk_size).await {
        Ok(data) if !data.is_empty() => match encode_chunk_frame(index, filename, &data) {
            Some(frame) => vec![Frame::Binary(frame)],
            None => vec![error_frame(format!("Cannot encode chunk {filename}:{index}"))],
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

    fn temp_root() -> tempfile::TempDir {
        tempfile::tempdir().unwrap()
    }

    #[test]
    fn resolve_uses_basename_and_probes_subdirs() {
        let root = temp_root();
        fs::create_dir_all(root.path().join("assets")).unwrap();
        fs::write(root.path().join("assets/UI.nx"), b"data").unwrap();
        fs::write(root.path().join("root.nx"), b"root").unwrap();

        assert_eq!(
            resolve_file(root.path(), "../../evil.nx"),
            root.path().join("evil.nx")
        );
        assert_eq!(
            resolve_file(root.path(), "sub/dir/UI.nx"),
            root.path().join("assets/UI.nx")
        );
        assert_eq!(resolve_file(root.path(), "root.nx"), root.path().join("root.nx"));
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

        let frames = process_message(root.path(), r#"{"type":"get_size","file":"test.nx"}"#).await;
        assert_eq!(frames.len(), 1);
        let Frame::Text(text) = &frames[0] else { panic!() };
        let value: Value = serde_json::from_str(text).unwrap();
        assert_eq!(value["type"], "size");
        assert_eq!(value["size"], 100);
        assert!(value["version"].as_i64().unwrap() > 0);

        let frames = process_message(
            root.path(),
            r#"{"type":"get_chunks","file":"test.nx","start":0,"end":1,"chunk_size":60}"#,
        )
        .await;
        // Two chunk frames plus chunks_done.
        assert_eq!(frames.len(), 3);
        let Frame::Binary(first) = &frames[0] else { panic!() };
        assert_eq!(&first[5 + 7..], &contents[..60]);
        let Frame::Binary(second) = &frames[1] else { panic!() };
        assert_eq!(&second[5 + 7..], &contents[60..]);
        let Frame::Text(done) = &frames[2] else { panic!() };
        assert!(done.contains("chunks_done"));
    }

    #[tokio::test]
    async fn process_errors() {
        let root = temp_root();
        let frames = process_message(root.path(), "not json").await;
        let Frame::Text(text) = &frames[0] else { panic!() };
        assert!(text.contains("Invalid JSON"));

        let frames = process_message(root.path(), r#"{"type":"nope"}"#).await;
        let Frame::Text(text) = &frames[0] else { panic!() };
        assert!(text.contains("Unknown message type: nope"));

        let frames =
            process_message(root.path(), r#"{"type":"get_chunk","file":"missing.nx","index":0}"#)
                .await;
        let Frame::Text(text) = &frames[0] else { panic!() };
        assert!(text.contains("Chunk not found"));

        let frames = process_message(root.path(), r#"{"type":"get_size","file":"missing.nx"}"#).await;
        let Frame::Text(text) = &frames[0] else { panic!() };
        let value: Value = serde_json::from_str(text).unwrap();
        assert_eq!(value["size"], -1);
        assert_eq!(value["version"], -1);
    }
}
