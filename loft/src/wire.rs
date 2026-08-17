//! Types on the wire between this terminator and dovecote (the PidgeIoT
//! backend), defined here rather than imported: dovecote lives in another
//! repository and compiles to wasm for Cloudflare Workers, so a shared
//! crate would drag a Workers-shaped dependency into a plain native
//! service for a handful of fields. The paired definitions live in
//! dovecote's `capsules` crate; the JSON contract itself is specified in
//! the PidgeIoT repository's `docs/api.md`, which is the authority when
//! the two disagree.

use serde::Deserialize;

/// Body of a successful `GET /internal/coap-psk/:identity`. `identity` is
/// the pigeon's Durable Object id, `secret` the short PSK its handshake is
/// keyed with, and `token` the pigeon's device bearer token, which this
/// process presents on every proxied `/device/pigeons/:id/*` request.
///
/// `identity` is never read here (the caller already knows which identity
/// it asked about) and is kept only so this struct states the whole
/// message shape; unknown fields are ignored, so dovecote can add to the
/// response without breaking an older terminator.
#[derive(Debug, Clone, Deserialize)]
pub struct CoapPskLookup {
  #[allow(dead_code)]
  pub identity: String,
  pub secret: String,
  pub token: String,
}
