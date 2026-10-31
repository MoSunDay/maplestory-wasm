#!/usr/bin/env python3
"""Range-capable static file server for private e2e stacks.

Chromium streams large WASM artifacts with HTTP Range requests; the stdlib
``python3 -m http.server`` answers those with 200 + full body, which makes
chunked wasm downloads fail mid-flight. This server answers 206/416 style
partial content and is intentionally dependency-free.

Usage:
    python3 rangesrv.py [ROOT] [PORT]        # defaults: ./  8899

Kept class-free: sockets are served by plain functions on daemon threads.
"""
import os
import re
import socket
import sys
import threading

CHUNK = 65536


def guess_ctype(path):
    if path.endswith('.wasm'):
        return 'application/wasm'
    if path.endswith('.js'):
        return 'application/javascript'
    if path.endswith('.html'):
        return 'text/html'
    if path.endswith('.css'):
        return 'text/css'
    if path.endswith('.png'):
        return 'image/png'
    return 'application/octet-stream'


def parse_range(header, size):
    """Return (start, end) inclusive, or None when the header is absent."""
    if not header:
        return None
    m = re.match(r'bytes=(\d*)-(\d*)', header)
    if not m:
        return None
    start = int(m.group(1) or 0)
    end = int(m.group(2) or size - 1)
    return (start, min(end, size - 1))


def read_request(conn):
    data = b''
    while b'\r\n\r\n' not in data:
        part = conn.recv(4096)
        if not part:
            return None, {}
        data += part
        if len(data) > 65536:
            return None, {}
    head = data.decode('latin-1', 'replace')
    lines = head.split('\r\n')
    headers = {}
    for line in lines[1:]:
        if ':' in line:
            k, v = line.split(':', 1)
            headers[k.strip().lower()] = v.strip()
    return lines[0], headers


def send_file(conn, path, rng):
    size = os.path.getsize(path)
    ctype = guess_ctype(path)
    with open(path, 'rb') as f:
        if rng:
            start, end = rng
            if start >= size:
                conn.sendall(
                    b'HTTP/1.1 416 Range Not Satisfiable\r\n'
                    b'Content-Range: bytes */' + str(size).encode() + b'\r\n\r\n')
                return
            f.seek(start)
            length = end - start + 1
            status = 'HTTP/1.1 206 Partial Content'
            extra = ('Content-Range: bytes %d-%d/%d\r\n' % (start, end, size))
        else:
            length = size
            status = 'HTTP/1.1 200 OK'
            extra = ''
        head = ('%s\r\nAccept-Ranges: bytes\r\nContent-Length: %d\r\n'
                'Content-Type: %s\r\n%sConnection: close\r\n\r\n'
                % (status, length, ctype, extra))
        conn.sendall(head.encode())
        left = length
        while left > 0:
            chunk = f.read(min(CHUNK, left))
            if not chunk:
                break
            conn.sendall(chunk)
            left -= len(chunk)


def serve_conn(conn, root):
    try:
        request_line, headers = read_request(conn)
        if not request_line:
            return
        parts = request_line.split()
        if len(parts) < 2:
            conn.sendall(b'HTTP/1.1 400 Bad Request\r\n\r\n')
            return
        method, target = parts[0], parts[1]
        path = target.split('?', 1)[0].split('#', 1)[0]
        fs_path = os.path.abspath(os.path.join(root, path.lstrip('/')))
        if not (fs_path == root or fs_path.startswith(root + os.sep)):
            conn.sendall(b'HTTP/1.1 403 Forbidden\r\n\r\n')
            return
        if os.path.isdir(fs_path):
            fs_path = os.path.join(fs_path, 'index.html')
        if method not in ('GET', 'HEAD') or not os.path.isfile(fs_path):
            conn.sendall(b'HTTP/1.1 404 Not Found\r\n\r\n')
            return
        send_file(conn, fs_path, parse_range(headers.get('range'),
                                             os.path.getsize(fs_path)))
    except OSError:
        pass
    finally:
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        conn.close()


def main(argv):
    root = os.path.abspath(argv[1] if len(argv) > 1 else '.')
    port = int(argv[2] if len(argv) > 2 else 8899)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('0.0.0.0', port))
    srv.listen(16)
    sys.stderr.write('rangesrv: serving %s on %d\n' % (root, port))
    while True:
        conn, _addr = srv.accept()
        threading.Thread(target=serve_conn, args=(conn, root),
                         daemon=True).start()


if __name__ == '__main__':
    main(sys.argv)
