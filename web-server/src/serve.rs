//! HTTP connection handling: read requests off the socket, dispatch them,
//! and keep the connection alive when the client allows it.

use std::net::SocketAddr;
use std::path::Path;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tracing::{debug, warn};

use crate::respond;

/// Bound per-request memory while allowing large authentication/cookie headers.
/// The terminating CRLF pair is part of the limit, so exactly 10 MiB is valid.
const MAX_HEADER_SIZE: usize = 10 * 1024 * 1024;

/// One parsed HTTP request (only the parts this server cares about).
pub struct Request {
    pub method: String,
    /// Raw request target without the query string (kept for redirects).
    pub raw_path: String,
    /// Percent-decoded path without query/fragment.
    pub path: String,
    pub range: Option<String>,
    pub keep_alive: bool,
}

enum ReadOutcome {
    Request(Request),
    /// Client closed the connection cleanly.
    Closed,
    /// Unparseable or oversized request; reply 400 and hang up.
    BadRequest,
}

pub async fn handle_connection(mut stream: TcpStream, peer: SocketAddr, root: &Path) {
    let mut buffer: Vec<u8> = Vec::with_capacity(2048);
    loop {
        match read_request(&mut stream, &mut buffer).await {
            ReadOutcome::Request(request) => {
                let status = respond::respond(&mut stream, root, &request).await;
                debug!("\"{} {}\" {status} from {peer}", request.method, request.raw_path);
                if !request.keep_alive || status == 400 {
                    break;
                }
            }
            ReadOutcome::Closed => break,
            ReadOutcome::BadRequest => {
                warn!("[Server] Bad request from {peer}");
                let _ = stream
                    .write_all(b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                    .await;
                break;
            }
        }
    }
}

/// Read and parse one request. Leftover bytes after the header terminator are
/// kept in `buffer` for the next request on a keep-alive connection.
async fn read_request(stream: &mut TcpStream, buffer: &mut Vec<u8>) -> ReadOutcome {
    let header_end = loop {
        match bounded_header_end(buffer) {
            Ok(Some(pos)) => break pos,
            Ok(None) => {}
            Err(()) => return ReadOutcome::BadRequest,
        }
        let mut chunk = [0u8; 2048];
        match stream.read(&mut chunk).await {
            Ok(0) => {
                return if buffer.is_empty() {
                    ReadOutcome::Closed
                } else {
                    ReadOutcome::BadRequest
                };
            }
            Ok(count) => buffer.extend_from_slice(&chunk[..count]),
            Err(_) => return ReadOutcome::Closed,
        }
    };

    let total = header_end + 4;
    let outcome = parse_request(&buffer[..total]);
    buffer.drain(..total);
    outcome
}

fn find_header_end(buffer: &[u8]) -> Option<usize> {
    buffer.windows(4).position(|window| window == b"\r\n\r\n")
}

fn bounded_header_end(buffer: &[u8]) -> Result<Option<usize>, ()> {
    if let Some(pos) = find_header_end(buffer) {
        return (pos + 4 <= MAX_HEADER_SIZE)
            .then_some(Some(pos))
            .ok_or(());
    }
    (buffer.len() < MAX_HEADER_SIZE).then_some(None).ok_or(())
}

fn parse_request(head: &[u8]) -> ReadOutcome {
    let mut headers = [httparse::EMPTY_HEADER; 32];
    let mut request = httparse::Request::new(&mut headers);
    if !matches!(request.parse(head), Ok(httparse::Status::Complete(_))) {
        return ReadOutcome::BadRequest;
    }

    let (Some(method), Some(target), version) = (request.method, request.path, request.version)
    else {
        return ReadOutcome::BadRequest;
    };
    let method = method.to_owned();
    let target = target.to_owned();

    let mut range = None;
    let mut connection = String::new();
    for header in request.headers.iter() {
        if header.name.is_empty() {
            break;
        }
        if header.name.eq_ignore_ascii_case("range") {
            range = Some(String::from_utf8_lossy(header.value).into_owned());
        } else if header.name.eq_ignore_ascii_case("connection") {
            connection = String::from_utf8_lossy(header.value)
                .trim()
                .to_ascii_lowercase();
        }
    }

    // HTTP/1.1 defaults to keep-alive; HTTP/1.0 defaults to closing.
    let keep_alive = match version {
        Some(1) => connection != "close",
        _ => connection == "keep-alive",
    };

    // Strip query string and fragment before decoding the path.
    let raw_path = target.split(['?', '#']).next().unwrap_or("/").to_owned();
    let path = percent_decode(&raw_path);

    ReadOutcome::Request(Request {
        method,
        raw_path,
        path,
        range,
        keep_alive,
    })
}

/// Decode %XX escapes; invalid sequences pass through unchanged.
fn percent_decode(input: &str) -> String {
    fn hex_digit(byte: u8) -> Option<u8> {
        match byte {
            b'0'..=b'9' => Some(byte - b'0'),
            b'a'..=b'f' => Some(byte - b'a' + 10),
            b'A'..=b'F' => Some(byte - b'A' + 10),
            _ => None,
        }
    }

    let bytes = input.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let (Some(high), Some(low)) = (hex_digit(bytes[i + 1]), hex_digit(bytes[i + 2])) {
                out.push(high * 16 + low);
                i += 3;
                continue;
            }
        }
        out.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn percent_decode_handles_escapes_and_passthrough() {
        assert_eq!(percent_decode("/a%20b/c"), "/a b/c");
        assert_eq!(percent_decode("/%2E%2E/x"), "/../x");
        assert_eq!(percent_decode("/bad%ZZ"), "/bad%ZZ");
        assert_eq!(percent_decode("/plain"), "/plain");
    }

    #[test]
    fn header_end_detection() {
        assert_eq!(find_header_end(b"GET / HTTP/1.1\r\n\r\n"), Some(14));
        assert_eq!(find_header_end(b"GET /"), None);
    }

    #[test]
    fn header_size_limit_accepts_ten_mib_and_rejects_larger_blocks() {
        let prefix = b"GET / HTTP/1.1\r\nX-Large: ";
        let suffix = b"\r\n\r\n";
        let mut at_limit = Vec::with_capacity(MAX_HEADER_SIZE);
        at_limit.extend_from_slice(prefix);
        at_limit.resize(MAX_HEADER_SIZE - suffix.len(), b'a');
        at_limit.extend_from_slice(suffix);
        assert_eq!(bounded_header_end(&at_limit), Ok(Some(MAX_HEADER_SIZE - 4)));

        let mut over_limit = Vec::with_capacity(MAX_HEADER_SIZE + 1);
        over_limit.extend_from_slice(prefix);
        over_limit.resize(MAX_HEADER_SIZE + 1 - suffix.len(), b'a');
        over_limit.extend_from_slice(suffix);
        assert_eq!(bounded_header_end(&over_limit), Err(()));

        assert_eq!(bounded_header_end(&vec![b'a'; MAX_HEADER_SIZE]), Err(()));
    }
}
