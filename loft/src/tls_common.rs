//! Shared OpenSSL PSK server configuration for both listeners.
//!
//! PSK, not certificates: RFC 8323 with PSK ciphersuites needs no server
//! certificate at all (the shared key authenticates both sides), matching
//! the `~/pigeon` Zephyr client's `TLS_CREDENTIAL_PSK` setup and the
//! constrained-device norm (no clock, no CA store, no X.509 parsing in a
//! minimal mbedTLS build). TLS is pinned to 1.2: the PSK *ciphersuite*
//! family (TLS_PSK_WITH_AES_128_CCM_8 etc.) is a TLS 1.2 concept, and
//! TLS 1.3's external-PSK story is a different mechanism constrained
//! stacks don't speak yet.

use std::sync::{Arc, LazyLock};

use openssl::error::ErrorStack;
use openssl::ex_data::Index;
use openssl::ssl::{Ssl, SslContext, SslContextBuilder, SslMethod, SslVersion};

use crate::psk::{PskEntry, PskResolver};

/// Constrained-device PSK suites, most-preferred first. CCM_8 (8-byte tag)
/// is the CoAP/cellular-IoT standard suite; GCM and CBC variants cover
/// clients built without CCM.
pub const PSK_CIPHER_LIST: &str = "PSK-AES128-CCM8:PSK-AES128-GCM-SHA256:PSK-AES128-CBC-SHA256";

/// After a successful PSK exchange the callback stashes
/// (identity, bearer token) here so the post-handshake code can build its
/// `DeviceSession` from what was actually authenticated.
pub static SESSION_EX_INDEX: LazyLock<Index<Ssl, (String, String)>> =
  LazyLock::new(|| Ssl::new_ex_index().expect("ssl ex index"));

/// Builds a PSK-only server context for either method (`SslMethod::dtls()`
/// or `SslMethod::tls_server()`).
pub fn build_psk_server_context(
  method: SslMethod,
  is_dtls: bool,
  resolver: Arc<PskResolver>,
) -> Result<SslContextBuilder, ErrorStack> {
  let mut builder = SslContext::builder(method)?;

  // Pinned to 1.2 at both ends on both transports: the classic PSK
  // ciphersuites live there, and 1.3's external-PSK mechanism
  // (psk_key_exchange_modes / session-ticket shaped) is a different thing
  // constrained stacks don't speak.
  let version = if is_dtls {
    SslVersion::DTLS1_2
  } else {
    SslVersion::TLS1_2
  };
  builder.set_min_proto_version(Some(version))?;
  builder.set_max_proto_version(Some(version))?;

  builder.set_cipher_list(PSK_CIPHER_LIST)?;

  // Level 0, not OpenSSL's default: CCM_8's 64-bit tag rates below level
  // 1's 80-bit floor, so at any higher level the suite is listed but never
  // selected -- a CCM8-only client fails with "no shared cipher", and
  // `openssl ciphers` cannot tell, since the level applies at selection,
  // not listing. Scoped to PSK by construction: this context carries no
  // certificate, admits only the suites above, and is version-pinned, so
  // the level has nothing else left to decide.
  builder.set_security_level(0);

  builder.set_psk_server_callback(move |ssl, identity, psk_out| {
    psk_callback(&resolver, ssl, identity, psk_out)
  });

  Ok(builder)
}

/// Stack-neutral identity resolution shared by every PSK callback (the
/// OpenSSL DTLS/TCP callbacks here and the mbedTLS callback in the CID
/// listener), so reject semantics cannot drift between them: a non-UTF-8
/// identity, a resolver miss, and a resolver error all collapse to the
/// same `None`.
pub fn resolve_psk_identity(resolver: &PskResolver, identity: &[u8]) -> Option<(String, PskEntry)> {
  let Ok(identity) = std::str::from_utf8(identity) else {
    tracing::debug!("rejecting non-UTF-8 PSK identity");
    return None;
  };
  // Cached (60s positive TTL) blocking lookup against dovecote. Runs on
  // the connection's own OS thread -- never on the tokio runtime.
  match resolver.resolve(identity) {
    Some(entry) => Some((identity.to_string(), entry)),
    None => {
      tracing::info!(identity, "PSK identity rejected");
      None
    }
  }
}

fn psk_callback(
  resolver: &PskResolver,
  ssl: &mut openssl::ssl::SslRef,
  identity: Option<&[u8]>,
  psk_out: &mut [u8],
) -> Result<usize, ErrorStack> {
  // Returning Ok(0) aborts the handshake without a distinguishable
  // "unknown identity" signal to the peer -- deliberate; a probe learns
  // nothing beyond "handshake failed".
  let Some(identity) = identity else {
    return Ok(0);
  };
  let Some((identity, entry)) = resolve_psk_identity(resolver, identity) else {
    return Ok(0);
  };

  // PSK bytes convention: the raw UTF-8 bytes of the secret string,
  // matching the device side (Zephyr `tls_credential_add(...,
  // TLS_CREDENTIAL_PSK, secret, strlen(secret))` in ~/pigeon).
  let len = entry.psk.len();
  if len > psk_out.len() {
    tracing::error!(identity, "PSK secret longer than OpenSSL's PSK buffer");
    return Ok(0);
  }
  psk_out[..len].copy_from_slice(entry.psk.as_bytes());

  ssl.set_ex_data(*SESSION_EX_INDEX, (identity, entry.token));
  Ok(len)
}

/// Pulls the handshake-authenticated (identity, bearer token) pair off a
/// completed connection.
pub fn authenticated_session(ssl: &openssl::ssl::SslRef) -> Option<(String, String)> {
  ssl.ex_data(*SESSION_EX_INDEX).cloned()
}

#[cfg(test)]
mod tests {
  use super::*;
  use crate::psk::PskEntry;
  use std::net::{TcpListener, TcpStream};
  use std::time::Duration;

  const TEST_IDENTITY: &str = "test-pigeon";
  const TEST_PSK: &str = "0123456789abcdef0123456789abcdef";
  const CCM8: &str = "PSK-AES128-CCM8";
  const GCM: &str = "PSK-AES128-GCM-SHA256";
  const CBC: &str = "PSK-AES128-CBC-SHA256";

  fn resolver() -> Arc<PskResolver> {
    Arc::new(PskResolver::new(
      Box::new(|identity: &str| {
        Ok((identity == TEST_IDENTITY).then(|| PskEntry {
          psk: TEST_PSK.to_string(),
          token: "test-token".to_string(),
        }))
      }),
      Duration::from_secs(60),
    ))
  }

  fn server_context() -> SslContextBuilder {
    build_psk_server_context(SslMethod::tls_server(), false, resolver()).expect("server context")
  }

  /// One loopback TLS/TCP handshake against `server`, from a client
  /// offering exactly `offer`, returning the suite the server negotiated
  /// or its handshake error. The client's own security level would refuse
  /// to offer CCM8 too, so the offer is pinned to level 0 on that side --
  /// the same `@SECLEVEL=0` an `openssl s_client` probe needs.
  fn negotiate(server: SslContext, offer: &str) -> Result<String, String> {
    let listener = TcpListener::bind("127.0.0.1:0").expect("bind");
    let addr = listener.local_addr().expect("addr");
    let accept = std::thread::spawn(move || {
      let (tcp, _) = listener.accept().expect("accept");
      tcp
        .set_read_timeout(Some(Duration::from_secs(5)))
        .expect("read timeout");
      let ssl = Ssl::new(&server).expect("server ssl");
      match ssl.accept(tcp) {
        Ok(stream) => Ok(
          stream
            .ssl()
            .current_cipher()
            .map(|c| c.name().to_string())
            .unwrap_or_default(),
        ),
        Err(e) => Err(e.to_string()),
      }
    });

    let mut builder = SslContext::builder(SslMethod::tls_client()).expect("client ctx");
    builder
      .set_max_proto_version(Some(SslVersion::TLS1_2))
      .expect("max version");
    builder
      .set_cipher_list(&format!("{offer}:@SECLEVEL=0"))
      .expect("cipher list");
    builder.set_psk_client_callback(|_ssl, _hint, identity_out, psk_out| {
      identity_out[..TEST_IDENTITY.len()].copy_from_slice(TEST_IDENTITY.as_bytes());
      identity_out[TEST_IDENTITY.len()] = 0;
      psk_out[..TEST_PSK.len()].copy_from_slice(TEST_PSK.as_bytes());
      Ok(TEST_PSK.len())
    });
    let ctx = builder.build();
    let tcp = TcpStream::connect(addr).expect("connect");
    tcp
      .set_read_timeout(Some(Duration::from_secs(5)))
      .expect("read timeout");
    let ssl = Ssl::new(&ctx).expect("client ssl");
    // The server's verdict is the one under test; a refused client just
    // sees the alert.
    let _client = ssl.connect(tcp);
    accept.join().expect("accept thread")
  }

  /// Every suite in the pinned list must be selected when it is all a
  /// client offers -- listing a suite is not serving it. CCM8 offered
  /// ahead of GCM must also land on CCM8: the listener follows the
  /// client's order, and GCM there would mean CCM8 was excluded.
  #[test]
  fn psk_suites_negotiate_as_offered() {
    for (offer, want) in [
      (CCM8, CCM8),
      (GCM, GCM),
      (CBC, CBC),
      (&*format!("{CCM8}:{GCM}"), CCM8),
    ] {
      let got = negotiate(server_context().build(), offer);
      assert_eq!(got.as_deref(), Ok(want), "offer {offer}");
    }
  }

  /// The matrix above can see the failure it guards against: at the
  /// security level OpenSSL defaults to, the same context refuses a
  /// CCM8-only client with no shared cipher while still serving GCM.
  #[test]
  fn the_default_security_level_refuses_ccm8() {
    let mut builder = server_context();
    builder.set_security_level(1);
    let ctx = builder.build();
    let refused = negotiate(ctx.clone(), CCM8).expect_err("CCM8 alone must fail at level 1");
    assert!(
      refused.contains("no shared cipher"),
      "unexpected failure shape: {refused}"
    );
    assert_eq!(negotiate(ctx, GCM).as_deref(), Ok(GCM));
  }

  /// The level-0 argument rests on this context admitting nothing but PSK
  /// suites; a certificate suite added here would need its own floor.
  #[test]
  fn cipher_list_admits_only_psk_suites() {
    for suite in PSK_CIPHER_LIST.split(':') {
      assert!(suite.starts_with("PSK-"), "{suite} is not a PSK suite");
    }
  }
}
