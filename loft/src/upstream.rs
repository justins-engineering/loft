//! Upstream HTTP client for dovecote's device routes. After a PSK
//! handshake, the terminator acts as an ordinary device-side HTTP client:
//! `Authorization: Bearer <token>` on `/device/pigeons/:id/*`, where
//! `token` is the pigeon's device bearer token -- a distinct credential
//! from the PSK secret that keyed the handshake, minted together and
//! rotated together by dovecote. The PSK only proves the peer is
//! this pigeon; the bearer token is what actually authorizes each
//! upstream call, and the owning Durable Object verifies it per-request
//! exactly as it does for direct HTTPS devices. Nothing here weakens or
//! bypasses device auth.

use std::time::Duration;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Method {
  Get,
  Post,
}

/// The subset of an upstream HTTP response the CoAP layer needs.
#[derive(Debug, Default, Clone)]
pub struct UpstreamResponse {
  pub status: u16,
  pub body: Vec<u8>,
  /// `Content-Range` (firmware 206s).
  pub content_range: Option<String>,
  /// `X-Firmware-Size` (total image bytes).
  pub firmware_size: Option<u64>,
  /// `X-Firmware-Sha256` (hex).
  pub firmware_sha256: Option<String>,
}

/// Async upstream abstraction so the handler is unit-testable without a
/// network. The single implementation is `Dovecote` below.
pub trait Upstream: Send + Sync {
  fn device_request(
    &self,
    method: Method,
    pigeon_id: &str,
    leaf: &str,
    bearer: &str,
    range: Option<(u64, u64)>,
    body: Option<(Vec<u8>, &'static str)>,
  ) -> impl std::future::Future<Output = Result<UpstreamResponse, String>> + Send;
}

pub struct Dovecote {
  client: reqwest::Client,
  base_url: String,
}

impl Dovecote {
  /// Builds the client for `base_url`. Redirects are not followed: dovecote's
  /// device routes never redirect, and a followed one turns a POST into a GET
  /// of some other page whose 200 would read as the device's report landing.
  pub fn new(base_url: &str) -> Result<Dovecote, String> {
    let client = reqwest::Client::builder()
      .redirect(reqwest::redirect::Policy::none())
      // Distinctive UA -- the PidgeIoT repo's docs/api.md has a device-auth
      // troubleshooting note for this:
      // default library UAs can trip edge bot heuristics into HTML 403s.
      .user_agent(concat!("loft/", env!("CARGO_PKG_VERSION")))
      .timeout(Duration::from_secs(30))
      .connect_timeout(Duration::from_secs(10))
      .build()
      .map_err(|e| format!("upstream client build: {e}"))?;
    Ok(Dovecote {
      client,
      base_url: base_url.trim_end_matches('/').to_string(),
    })
  }
}

impl Upstream for Dovecote {
  async fn device_request(
    &self,
    method: Method,
    pigeon_id: &str,
    leaf: &str,
    bearer: &str,
    range: Option<(u64, u64)>,
    body: Option<(Vec<u8>, &'static str)>,
  ) -> Result<UpstreamResponse, String> {
    let url = format!("{}/device/pigeons/{}/{}", self.base_url, pigeon_id, leaf);

    let mut req = match method {
      Method::Get => self.client.get(&url),
      Method::Post => self.client.post(&url),
    };
    req = req.header("Authorization", format!("Bearer {bearer}"));
    if let Some((start, end)) = range {
      req = req.header("Range", format!("bytes={start}-{end}"));
    }
    if let Some((bytes, content_type)) = body {
      req = req.header("Content-Type", content_type).body(bytes);
    }

    let resp = req.send().await.map_err(|e| format!("upstream: {e}"))?;

    let status = resp.status().as_u16();
    let header = |name: &str| {
      resp
        .headers()
        .get(name)
        .and_then(|v| v.to_str().ok())
        .map(str::to_string)
    };
    let content_range = header("Content-Range");
    let firmware_size = header("X-Firmware-Size").and_then(|v| v.parse().ok());
    let firmware_sha256 = header("X-Firmware-Sha256");

    let body = resp
      .bytes()
      .await
      .map_err(|e| format!("upstream body: {e}"))?
      .to_vec();

    Ok(UpstreamResponse {
      status,
      body,
      content_range,
      firmware_size,
      firmware_sha256,
    })
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  use std::io::{Read, Write};
  use std::net::TcpListener;

  /// A 302 from in front of dovecote reaches the handler as a 302, not as
  /// the 200 of the page it points at.
  #[tokio::test]
  async fn redirect_is_not_followed() {
    let listener = TcpListener::bind("127.0.0.1:0").expect("bind");
    let addr = listener.local_addr().expect("addr");
    std::thread::spawn(move || {
      for stream in listener.incoming() {
        let Ok(mut stream) = stream else { return };
        let mut buf = [0u8; 4096];
        let n = stream.read(&mut buf).unwrap_or(0);
        let reply: &[u8] = if buf[..n].starts_with(b"GET /login") {
          b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\n<html>"
        } else {
          b"HTTP/1.1 302 Found\r\nLocation: /login\r\nContent-Length: 0\r\n\
            Connection: close\r\n\r\n"
        };
        let _ = stream.write_all(reply);
      }
    });

    let mut base = String::with_capacity(32);
    base.push_str("http://");
    base.push_str(&addr.to_string());
    let upstream = Dovecote::new(&base).expect("client");
    let resp = upstream
      .device_request(
        Method::Post,
        "pigeon-1",
        "telemetry",
        "token",
        None,
        Some((b"{}".to_vec(), "application/json")),
      )
      .await
      .expect("response");
    assert_eq!(resp.status, 302);
  }
}
