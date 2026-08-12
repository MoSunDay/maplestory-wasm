use std::net::{Ipv6Addr, SocketAddr};

use socket2::{Domain, Protocol, Socket, Type};
use tokio::net::TcpListener;

/// Bind a listener, making the default IPv6 wildcard explicitly dual-stack.
/// Explicit non-wildcard bind addresses retain their single-family behavior.
pub async fn bind_listener(bind: &str, port: u16) -> std::io::Result<TcpListener> {
    if bind != "::" {
        return TcpListener::bind((bind, port)).await;
    }

    let socket = Socket::new(Domain::IPV6, Type::STREAM, Some(Protocol::TCP))?;
    socket.set_only_v6(false)?;
    socket.set_reuse_address(true)?;
    socket.bind(&SocketAddr::from((Ipv6Addr::UNSPECIFIED, port)).into())?;
    socket.listen(128)?;
    socket.set_nonblocking(true)?;
    TcpListener::from_std(socket.into())
}

#[cfg(test)]
mod tests {
    use tokio::net::TcpStream;

    use super::bind_listener;

    #[tokio::test]
    async fn wildcard_listener_accepts_ipv4_and_ipv6() {
        let listener = bind_listener("::", 0).await.unwrap();
        let port = listener.local_addr().unwrap().port();

        let ipv4 = TcpStream::connect(("127.0.0.1", port));
        let (_, peer4) = tokio::join!(ipv4, listener.accept());
        assert!(peer4.unwrap().1.ip().is_ipv6());

        let ipv6 = TcpStream::connect(("::1", port));
        let (_, peer6) = tokio::join!(ipv6, listener.accept());
        assert!(peer6.unwrap().1.ip().is_ipv6());
    }
}
