#!/usr/bin/env python3
"""Range-capable static file server for clean-HEAD e2e validation.

python3 -m http.server 对 Range 请求返回 200 全量，而客户端分片 wasm 加载器
（web/index.html）要求 206 Partial Content，否则 3 次重试后初始化失败
（"WASM module did not initialize"）。本脚本提供最小 206 支持：

    python3 serve_clean.py <port> <root>   # 例: serve_clean.py 8010 /tmp/msw-clean
"""
import os
import socket
import sys
import threading

TYPES = {
    ".js": "application/javascript", ".wasm": "application/wasm",
    ".html": "text/html; charset=utf-8", ".css": "text/css",
    ".json": "application/json", ".png": "image/png", ".ico": "image/x-icon",
    ".map": "application/json", ".nx": "application/octet-stream",
}


def resolve(root, target):
    path = target.split("?", 1)[0]
    if path.endswith("/"):
        path += "index.html"
    full = os.path.normpath(os.path.join(root, path.lstrip("/")))
    base = os.path.abspath(root)
    if full != base and not full.startswith(base + os.sep):
        return None  # 目录穿越
    return full if os.path.isfile(full) else None


def parse_range(header, size):
    """返回 (start, end) / None（无 Range）/ 'invalid'（416）。"""
    if not header or not header.startswith("bytes="):
        return None
    first, _, last = header[len("bytes="):].partition("-")
    if not first:  # 后缀 -N
        n = int(last or "0")
        return (max(0, size - n), size - 1) if n > 0 else "invalid"
    start = int(first)
    end = int(last) if last else size - 1
    if start >= size or start > end:
        return "invalid"
    return (start, min(end, size - 1))


def response_head(status, ctype, length, extra=""):
    return (f"HTTP/1.1 {status}\r\nContent-Type: {ctype}\r\n"
            f"Content-Length: {length}\r\nAccept-Ranges: bytes\r\n"
            f"Connection: close\r\n{extra}\r\n").encode()


def read_request(conn):
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(65536)
        if not chunk:
            return None
        data += chunk
        if len(data) > 1 << 20:
            return None
    head = data.split(b"\r\n\r\n", 1)[0].decode("latin-1")
    lines = head.split("\r\n")
    method, target, _ = lines[0].split(" ", 2)
    headers = {}
    for line in lines[1:]:
        key, _, value = line.partition(":")
        headers[key.strip().lower()] = value.strip()
    return method, target, headers


def serve_conn(conn, root):
    try:
        request = read_request(conn)
        if not request:
            conn.sendall(response_head("400 Bad Request", "text/plain", 0))
            return
        method, target, headers = request
        path = resolve(root, target)
        if not path:
            conn.sendall(response_head("404 Not Found", "text/plain", 0))
            return
        size = os.path.getsize(path)
        ctype = TYPES.get(os.path.splitext(path)[1], "application/octet-stream")
        rng = parse_range(headers.get("range"), size)
        if rng == "invalid":
            conn.sendall(response_head(
                "416 Range Not Satisfiable", "text/plain", 0,
                extra=f"Content-Range: bytes */{size}\r\n"))
            return
        if rng:
            start, end = rng
            status = "206 Partial Content"
            extra = f"Content-Range: bytes {start}-{end}/{size}\r\n"
        else:
            start, end, status, extra = 0, size - 1, "200 OK", ""
        length = end - start + 1
        if method == "HEAD":
            conn.sendall(response_head(status, ctype, length, extra))
            return
        conn.sendall(response_head(status, ctype, length, extra))
        with open(path, "rb") as body:
            body.seek(start)
            remaining = length
            while remaining > 0:
                buf = body.read(min(1 << 16, remaining))
                if not buf:
                    break
                conn.sendall(buf)
                remaining -= len(buf)
    except OSError:
        pass
    finally:
        conn.close()


def run(port, root):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(64)
    print(f"serve_clean: :{port} root={os.path.abspath(root)}", flush=True)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=serve_conn, args=(conn, root), daemon=True).start()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8010
    root = sys.argv[2] if len(sys.argv) > 2 else os.getcwd()
    run(port, root)


if __name__ == "__main__":
    main()
