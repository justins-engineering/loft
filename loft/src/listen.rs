//! Listener sockets for the three listeners. std's `bind` leaves one
//! thing to the OS: whether an IPv6 socket also carries IPv4
//! (`IPV6_V6ONLY`, a sysctl default on Linux). A `[::]` bind address is
//! made dual-stack explicitly here, so the device host's A and AAAA
//! records land on one socket and a session can follow a device across
//! families (a Connection ID rebind, on the mbedTLS listener). An IPv4
//! bind address goes through std untouched, so the default deployment is
//! byte-for-byte what it was.

use std::io;
use std::net::{SocketAddr, TcpListener, UdpSocket};

use socket2::{Domain, Protocol, Socket, Type};

/// Binds a DTLS listener socket. An IPv6 literal is dual-stack; anything
/// else binds exactly as std would (a hostname still resolves).
pub fn bind_udp(addr: &str) -> io::Result<UdpSocket> {
  match ipv6_literal(addr) {
    Some(v6) => {
      let sock = dual_stack(Type::DGRAM, Protocol::UDP)?;
      sock.bind(&v6.into())?;
      Ok(sock.into())
    }
    None => UdpSocket::bind(addr),
  }
}

/// Binds a TLS/TCP listener. Same family rule as [`bind_udp`].
pub fn bind_tcp(addr: &str) -> io::Result<TcpListener> {
  match ipv6_literal(addr) {
    Some(v6) => {
      let sock = dual_stack(Type::STREAM, Protocol::TCP)?;
      // std sets this on the listeners it creates; a restart must not
      // wait out TIME_WAIT on the old port.
      sock.set_reuse_address(true)?;
      sock.bind(&v6.into())?;
      // std's backlog.
      sock.listen(128)?;
      Ok(sock.into())
    }
    None => TcpListener::bind(addr),
  }
}

fn ipv6_literal(addr: &str) -> Option<SocketAddr> {
  addr.parse::<SocketAddr>().ok().filter(|a| a.is_ipv6())
}

fn dual_stack(ty: Type, proto: Protocol) -> io::Result<Socket> {
  let sock = Socket::new(Domain::IPV6, ty, Some(proto))?;
  sock.set_only_v6(false)?;
  Ok(sock)
}

/// A peer as the listeners key, count, and log it. An IPv4 source seen
/// through a dual-stack socket arrives as `::ffff:a.b.c.d`; folding it
/// back keeps one host one peer whichever family observed it. Replies to
/// the folded address still leave through the IPv6 socket: Linux maps an
/// AF_INET destination on a dual-stack socket itself.
pub fn canonical_peer(peer: SocketAddr) -> SocketAddr {
  SocketAddr::new(peer.ip().to_canonical(), peer.port())
}

#[cfg(test)]
mod tests {
  use super::*;
  use std::io::{Read, Write};
  use std::net::{Ipv4Addr, Ipv6Addr, TcpStream};
  use std::time::Duration;

  #[test]
  fn canonical_peer_folds_a_mapped_v4_source_and_leaves_the_rest() {
    let mapped: SocketAddr = "[::ffff:192.0.2.1]:5684".parse().expect("addr");
    let v4: SocketAddr = "192.0.2.1:5684".parse().expect("addr");
    let v6: SocketAddr = "[2001:db8::1]:5684".parse().expect("addr");
    assert_eq!(canonical_peer(mapped), v4);
    assert_eq!(canonical_peer(v4), v4);
    assert_eq!(canonical_peer(v6), v6);
  }

  /// One `[::]` socket serves both families; a v4 source folds to its v4
  /// form, and a reply addressed to that folded form still reaches it.
  #[test]
  fn udp_ipv6_any_is_dual_stack() {
    let server = bind_udp("[::]:0").expect("bind dual-stack");
    server
      .set_read_timeout(Some(Duration::from_secs(2)))
      .expect("timeout");
    let port = server.local_addr().expect("addr").port();
    let mut buf = [0u8; 8];

    let v4 = UdpSocket::bind("127.0.0.1:0").expect("bind v4 client");
    v4.set_read_timeout(Some(Duration::from_secs(2)))
      .expect("timeout");
    v4.send_to(b"four", (Ipv4Addr::LOCALHOST, port))
      .expect("send v4");
    let (n, from) = server.recv_from(&mut buf).expect("recv v4");
    assert_eq!(&buf[..n], b"four");
    assert!(from.is_ipv6(), "the socket reports the mapped form");
    let from = canonical_peer(from);
    assert_eq!(from, v4.local_addr().expect("addr"));
    server
      .send_to(b"back", from)
      .expect("reply to the folded address");
    let (n, _) = v4.recv_from(&mut buf).expect("reply arrives");
    assert_eq!(&buf[..n], b"back");

    let v6 = UdpSocket::bind("[::1]:0").expect("bind v6 client");
    v6.send_to(b"six", (Ipv6Addr::LOCALHOST, port))
      .expect("send v6");
    let (n, from) = server.recv_from(&mut buf).expect("recv v6");
    assert_eq!(&buf[..n], b"six");
    assert_eq!(canonical_peer(from), v6.local_addr().expect("addr"));
  }

  #[test]
  fn tcp_ipv6_any_is_dual_stack() {
    let server = bind_tcp("[::]:0").expect("bind dual-stack");
    let port = server.local_addr().expect("addr").port();

    let mut v4 = TcpStream::connect((Ipv4Addr::LOCALHOST, port)).expect("connect v4");
    let (mut accepted, from) = server.accept().expect("accept v4");
    assert_eq!(canonical_peer(from), v4.local_addr().expect("addr"));
    accepted.write_all(b"hi").expect("write");
    let mut buf = [0u8; 2];
    v4.read_exact(&mut buf).expect("read");
    assert_eq!(&buf, b"hi");

    let v6 = TcpStream::connect((Ipv6Addr::LOCALHOST, port)).expect("connect v6");
    let (_, from) = server.accept().expect("accept v6");
    assert_eq!(canonical_peer(from), v6.local_addr().expect("addr"));
  }

  /// The default deployment's addresses take std's path: a v4 socket with
  /// no v6 face at all.
  #[test]
  fn ipv4_addresses_bind_as_ipv4() {
    let udp = bind_udp("127.0.0.1:0").expect("bind udp");
    assert!(udp.local_addr().expect("addr").is_ipv4());
    let tcp = bind_tcp("127.0.0.1:0").expect("bind tcp");
    assert!(tcp.local_addr().expect("addr").is_ipv4());
  }
}
