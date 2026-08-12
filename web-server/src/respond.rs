//! Response construction: path resolution, Range handling, file streaming,
//! and the shared CORS/COOP/COEP header set the WASM client depends on.

use std::path::{Path, PathBuf};

use tokio::io::{AsyncReadExt, AsyncSeekExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tracing::{error, info};

use crate::serve::Request;

/// Size of one streaming read while sending a file body.
const BODY_CHUNK_SIZE: usize = 64 * 1024;

/// Handle one request; returns the HTTP status sent (for logging).
pub async fn respond(stream: &mut TcpStream, root: &Path, req: &Request) -> u16 {
    if req.method != "GET" && req.method != "HEAD" {
        send_simple(
            stream,
            405,
            "Method Not Allowed",
            &[("Allow", "GET, HEAD")],
            !req.keep_alive,
        )
        .await;
        return 405;
    }

    let Some(relative) = normalize_path(&req.path) else {
        send_simple(stream, 403, "Forbidden", &[], !req.keep_alive).await;
        return 403;
    };
    let full_path = root.join(&relative);

    // Follow symlinks like the old server did (metadata, not symlink_metadata).
    match tokio::fs::metadata(&full_path).await {
        Ok(meta) if meta.is_dir() => serve_directory(stream, req, &full_path).await,
        Ok(meta) => serve_file(stream, req, &full_path, meta.len()).await,
        Err(_) => {
            send_simple(stream, 404, "Not Found", &[], !req.keep_alive).await;
            404
        }
    }
}

/// Serve a directory: redirect to the trailing-slash form, then prefer
/// index.html and fall back to a plain listing.
async fn serve_directory(stream: &mut TcpStream, req: &Request, dir: &Path) -> u16 {
    if !req.raw_path.ends_with('/') {
        let location = format!("{}/", req.raw_path);
        send_simple(
            stream,
            301,
            "Moved Permanently",
            &[("Location", &location)],
            !req.keep_alive,
        )
        .await;
        return 301;
    }

    let index_path = dir.join("index.html");
    if let Ok(meta) = tokio::fs::metadata(&index_path).await {
        if meta.is_file() {
            return serve_file(stream, req, &index_path, meta.len()).await;
        }
    }
    serve_listing(stream, req, dir).await
}

async fn serve_listing(stream: &mut TcpStream, req: &Request, dir: &Path) -> u16 {
    let display = escape_html(&req.path);
    let mut body = format!(
        "<!DOCTYPE html>\n<html><head><title>Directory listing for {display}</title></head>\n\
         <body>\n<h1>Directory listing for {display}</h1>\n<hr>\n<ul>\n"
    );

    let mut names: Vec<String> = Vec::new();
    if let Ok(mut dir_entries) = tokio::fs::read_dir(dir).await {
        while let Ok(Some(entry)) = dir_entries.next_entry().await {
            let name = entry.file_name().to_string_lossy().into_owned();
            let is_dir = entry
                .metadata()
                .await
                .map(|meta| meta.is_dir())
                .unwrap_or(false);
            names.push(if is_dir { format!("{name}/") } else { name });
        }
    }
    names.sort();
    for name in &names {
        let href = percent_encode(name);
        let escaped = escape_html(name);
        body.push_str(&format!("<li><a href=\"{href}\">{escaped}</a></li>\n"));
    }
    body.push_str("</ul>\n<hr>\n</body></html>\n");

    send_body(stream, req, "text/html; charset=utf-8", body.as_bytes()).await;
    200
}

async fn serve_file(stream: &mut TcpStream, req: &Request, path: &Path, size: u64) -> u16 {
    let content_type = content_type_of(path);
    // Range requests apply to GET only, mirroring the old server.
    let range_result = req
        .range
        .as_deref()
        .filter(|_| req.method == "GET")
        .map(|header| parse_range(header, size));

    match range_result {
        Some(Ok((start, end))) => {
            let length = end - start + 1;
            let extras = [
                ("Content-Range", format!("bytes {start}-{end}/{size}")),
                ("Content-Length", length.to_string()),
            ];
            let head = header_block(
                206,
                "Partial Content",
                content_type,
                &extras
                    .iter()
                    .map(|(name, value)| (*name, value.as_str()))
                    .collect::<Vec<_>>(),
                !req.keep_alive,
            );
            if stream.write_all(head.as_bytes()).await.is_ok() {
                stream_file(stream, path, start, length).await;
            }
            206
        }
        Some(Err(())) => {
            // 416 must carry Content-Range with the current full size.
            let extras = [
                ("Content-Range", format!("bytes */{size}")),
                ("Content-Length", "0".to_owned()),
            ];
            let head = header_block(
                416,
                "Range Not Satisfiable",
                content_type,
                &extras
                    .iter()
                    .map(|(name, value)| (*name, value.as_str()))
                    .collect::<Vec<_>>(),
                !req.keep_alive,
            );
            let _ = stream.write_all(head.as_bytes()).await;
            416
        }
        None => {
            let extras = [("Content-Length", size.to_string())];
            let head = header_block(
                200,
                "OK",
                content_type,
                &extras
                    .iter()
                    .map(|(name, value)| (*name, value.as_str()))
                    .collect::<Vec<_>>(),
                !req.keep_alive,
            );
            if stream.write_all(head.as_bytes()).await.is_ok() && req.method == "GET" {
                stream_file(stream, path, 0, size).await;
            }
            if req.method == "HEAD" {
                info!(
                    "[HEAD] {} - {} bytes ({:.2} MB)",
                    req.path,
                    size,
                    size as f64 / (1024.0 * 1024.0)
                );
            }
            200
        }
    }
}

/// Stream `length` bytes of `path` starting at `offset` to the socket.
async fn stream_file(stream: &mut TcpStream, path: &Path, offset: u64, length: u64) {
    let mut file = match tokio::fs::File::open(path).await {
        Ok(file) => file,
        Err(err) => {
            error!("[Server] Failed to open {}: {err}", path.display());
            return;
        }
    };
    if let Err(err) = file.seek(std::io::SeekFrom::Start(offset)).await {
        error!("[Server] Failed to seek {}: {err}", path.display());
        return;
    }
    let mut remaining = length;
    let mut chunk = vec![0u8; BODY_CHUNK_SIZE];
    while remaining > 0 {
        let want = (BODY_CHUNK_SIZE as u64).min(remaining) as usize;
        match file.read(&mut chunk[..want]).await {
            // File shrank between stat and read; stop instead of hanging.
            Ok(0) => break,
            Ok(count) => {
                if stream.write_all(&chunk[..count]).await.is_err() {
                    break;
                }
                remaining -= count as u64;
            }
            Err(_) => break,
        }
    }
}

/// Shared header block for every response. The isolation headers enable
/// SharedArrayBuffer in the browser, which threaded WASM requires.
fn header_block(
    status: u16,
    reason: &str,
    content_type: &str,
    extras: &[(&str, &str)],
    close: bool,
) -> String {
    let mut head = format!("HTTP/1.1 {status} {reason}\r\n");
    head.push_str("Cross-Origin-Opener-Policy: same-origin\r\n");
    head.push_str("Cross-Origin-Embedder-Policy: require-corp\r\n");
    head.push_str("Access-Control-Allow-Origin: *\r\n");
    head.push_str("Cache-Control: no-store, no-cache, must-revalidate\r\n");
    head.push_str("Accept-Ranges: bytes\r\n");
    head.push_str(&format!("Content-Type: {content_type}\r\n"));
    for (name, value) in extras {
        head.push_str(&format!("{name}: {value}\r\n"));
    }
    head.push_str(&format!(
        "Connection: {}\r\n",
        if close { "close" } else { "keep-alive" }
    ));
    head.push_str("\r\n");
    head
}

/// Small text responses (errors, redirects).
async fn send_simple(
    stream: &mut TcpStream,
    status: u16,
    reason: &str,
    extras: &[(&str, &str)],
    close: bool,
) {
    let body = format!("{status} {reason}\n");
    let mut owned: Vec<(String, String)> = extras
        .iter()
        .map(|(name, value)| (name.to_string(), value.to_string()))
        .collect();
    owned.push(("Content-Length".to_owned(), body.len().to_string()));
    let refs: Vec<(&str, &str)> = owned
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect();
    let head = header_block(status, reason, "text/plain; charset=utf-8", &refs, close);
    let _ = stream.write_all(head.as_bytes()).await;
    let _ = stream.write_all(body.as_bytes()).await;
}

/// Full-body response (directory listings).
async fn send_body(stream: &mut TcpStream, req: &Request, content_type: &str, body: &[u8]) {
    let extras = [("Content-Length", body.len().to_string())];
    let head = header_block(
        200,
        "OK",
        content_type,
        &extras
            .iter()
            .map(|(name, value)| (*name, value.as_str()))
            .collect::<Vec<_>>(),
        !req.keep_alive,
    );
    if stream.write_all(head.as_bytes()).await.is_err() || req.method != "GET" {
        return;
    }
    let _ = stream.write_all(body).await;
}

/// Turn an absolute URL path into root-relative segments.
///
/// Returns `None` when the path tries to escape the root via `..`.
pub fn normalize_path(path: &str) -> Option<PathBuf> {
    let mut relative = PathBuf::new();
    for segment in path.split('/') {
        match segment {
            "" | "." => {}
            ".." => return None,
            _ => relative.push(segment),
        }
    }
    Some(relative)
}

/// Parse an HTTP Range header (`bytes=START-END`, `bytes=START-`,
/// `bytes=-SUFFIX`). Mirrors the old server's single-range semantics.
pub fn parse_range(header: &str, file_size: u64) -> Result<(u64, u64), ()> {
    let spec = header.strip_prefix("bytes=").ok_or(())?;
    let mut parts = spec.split('-');
    let start_str = parts.next().ok_or(())?.trim();
    let end_str = parts.next().ok_or(())?.trim();
    // More than one dash means multiple ranges, which are unsupported.
    if parts.next().is_some() {
        return Err(());
    }

    let size = file_size as i64;
    let (mut start, mut end) = match (start_str.is_empty(), end_str.is_empty()) {
        (false, false) => (
            start_str.parse::<i64>().map_err(drop)?,
            end_str.parse::<i64>().map_err(drop)?,
        ),
        (false, true) => (start_str.parse::<i64>().map_err(drop)?, size - 1),
        (true, false) => {
            let suffix = end_str.parse::<i64>().map_err(drop)?;
            (size - suffix, size - 1)
        }
        (true, true) => return Err(()),
    };

    if start < 0 {
        start = 0;
    }
    if end >= size {
        end = size - 1;
    }
    if start > end {
        return Err(());
    }
    Ok((start as u64, end as u64))
}

/// MIME types the client actually needs; everything else is opaque bytes.
fn content_type_of(path: &Path) -> &'static str {
    match path.extension().and_then(|ext| ext.to_str()) {
        Some("html") | Some("htm") => "text/html",
        Some("js") => "application/javascript",
        Some("wasm") => "application/wasm",
        Some("css") => "text/css",
        Some("json") => "application/json",
        _ => "application/octet-stream",
    }
}

fn percent_encode(name: &str) -> String {
    let mut out = String::with_capacity(name.len());
    for byte in name.bytes() {
        match byte {
            b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' | b'/' => {
                out.push(byte as char)
            }
            _ => out.push_str(&format!("%{byte:02X}")),
        }
    }
    out
}

fn escape_html(text: &str) -> String {
    text.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn range_parsing_matches_old_semantics() {
        assert_eq!(parse_range("bytes=0-99", 250), Ok((0, 99)));
        assert_eq!(parse_range("bytes=100-", 250), Ok((100, 249)));
        assert_eq!(parse_range("bytes=-50", 250), Ok((200, 249)));
        assert_eq!(parse_range("bytes=-999", 250), Ok((0, 249)));
        assert_eq!(parse_range("bytes=200-999", 250), Ok((200, 249)));
        assert_eq!(parse_range("bytes=249-249", 250), Ok((249, 249)));
        assert!(parse_range("bytes=250-", 250).is_err());
        assert!(parse_range("bytes=100-50", 250).is_err());
        assert!(parse_range("bytes=abc", 250).is_err());
        assert!(parse_range("bytes=0-10,20-30", 250).is_err());
        assert!(parse_range("items=0-10", 250).is_err());
        assert!(parse_range("bytes=-", 250).is_err());
        assert!(parse_range("bytes=0-1", 0).is_err());
    }

    #[test]
    fn normalize_blocks_traversal() {
        assert_eq!(normalize_path("/a/b"), Some(PathBuf::from("a/b")));
        assert_eq!(normalize_path("/a/../b"), None);
        assert_eq!(normalize_path("/../x"), None);
        assert_eq!(normalize_path("/"), Some(PathBuf::new()));
        assert_eq!(normalize_path("/a//b/./c"), Some(PathBuf::from("a/b/c")));
    }

    #[test]
    fn content_types() {
        assert_eq!(content_type_of(Path::new("x/a.wasm")), "application/wasm");
        assert_eq!(content_type_of(Path::new("a.js")), "application/javascript");
        assert_eq!(content_type_of(Path::new("a.html")), "text/html");
        assert_eq!(
            content_type_of(Path::new("a.nx")),
            "application/octet-stream"
        );
    }
}
