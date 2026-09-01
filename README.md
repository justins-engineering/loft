# loft

`loft` is a CoAP terminator. It accepts CoAP from constrained devices over both secure
transports on one port, authenticates each device with a pre-shared key, and translates the
requests onto ordinary HTTP calls to a backend.

It exists because a Cloudflare Workers runtime is HTTP-only: it cannot terminate raw UDP or
CoAP framing. `loft` runs on an ordinary VPS in front of that backend and does the part the
edge cannot. [`pigeonhole`](https://github.com/justins-engineering/pigeonhole), the platform's
MQTT broker, is the same shape for a different protocol.

What it handles:

- **DTLS 1.2 over UDP** (`coaps://`, RFC 7252), the primary transport for power-saving
  cellular devices. Stateless HelloVerifyRequest cookie exchange before any per-peer state is
  allocated, and RFC 9146 Connection ID so a NAT rebind does not cost a new handshake.
- **TLS 1.2 over TCP** (`coaps+tcp://`, RFC 8323) on the same authority and port, for device
  builds compiled with the TCP transport.
- **PSK authentication only, no certificates.** PSK ciphersuites authenticate both directions,
  so a constrained client needs no CA store and no accurate clock. Identities resolve to
  secrets against the backend at handshake time, with a short positive and negative cache.
- **Block-wise transfer** (RFC 7959) in both directions, including firmware downloads served
  block by block out of an HTTP `Range` request against the backend.
- **Admission control**: global and per-source-address connection ceilings, bounded Block1
  reassembly buffers, and UDP duplicate detection.

Two DTLS stacks are compiled in and selected at runtime by `LOFT_DTLS_STACK`: OpenSSL, and
mbedTLS for the Connection ID listener. `docs/infra/coap-cid-design.md` covers why both exist
and how the migration between them is staged.

## Wire contract with the backend

The backend today is `dovecote`, the PidgeIoT edge Worker, which lives in the
[`pidgeiot`](https://github.com/justins-engineering/pidgeiot) repository. Two HTTP surfaces
connect the two services, and that repository's `docs/api.md` is the authority on both:

1. **PSK resolution.** `GET /internal/coap-psk/:identity`, authenticated by a service secret
   shared between the two processes (`COAP_SERVICE_SECRET`, the same value on both sides) and
   additionally gated by source address on the backend. The identity is the device's id; the
   response carries the short PSK that keys the handshake plus that device's own bearer token.
   The response shape is mirrored locally in `loft/src/wire.rs`, which names its paired
   definition in the backend.

2. **The device data path.** Every terminated CoAP request maps 1:1 onto the backend's
   `/device/pigeons/:id/*` HTTP routes, carrying the device's bearer token in an
   `Authorization` header. `loft` is not a trusted proxy in the authorization sense: the
   backend verifies that token cryptographically on every request, exactly as it does for a
   device speaking HTTPS directly. `loft` adds one check of its own, that the device id in the
   request path equals the handshake identity, so a device can never address another device's
   resources.

The PSK and the bearer token are minted together and rotated together by the backend, and are
deliberately different strings: the bearer token is longer than the 32 bytes that constrained
PSK stacks are guaranteed to accept.

## Build

```sh
cargo check          # OpenSSL stack only, no system mbedTLS needed
cargo test
```

The mbedTLS Connection ID stack is behind the `mbedtls` feature and links the system mbedTLS
3.6 shared library, so it is excluded from the workspace's default members. Every shipped
binary is dual-stack; the container build always enables the feature:

```sh
docker build -f loft/Dockerfile -t loft .
```

That build runs from the repository root as context. Its runtime stage verifies at image build
time that the mbedTLS shared library actually exports Connection ID support, and that the
OpenSSL it ships actually *selects* each PSK ciphersuite loft pins — by completing real
handshakes against the freshly built binary (`scripts/test/psk-suite-check.sh`), because a
cipher list can carry a suite the library never chooses, and either gap would silently fail
every affected device instead of failing the build.

## Deploy

Production runs the bare binary under systemd on a VPS. `infra/coap-terminator/loft.service`
is the unit, hardened against this specific process shape (stateless, two listeners, one
outbound HTTPS leg, one secret read through `LoadCredential=` rather than the environment).
`infra/coap-terminator/docker-compose.yml` is the supported container path for local
development and for deploying elsewhere.

Configuration is entirely environment variables (`LOFT_DOVECOTE_URL`, `LOFT_UDP_LISTEN`,
`LOFT_TCP_LISTEN`, `LOFT_PSK_TTL_SECS`, `LOFT_LOG`, `LOFT_DTLS_STACK`,
`LOFT_HANDSHAKE_DEADLINE_SECS`) plus the service
secret. The listeners default to IPv4; `[::]:5684` as a listen address binds dual-stack, IPv4
and IPv6 on one socket. `docs/infra/coap-terminator.md` is the full runbook: bring-up in both
deployment shapes, firewall rules for each, the IPv6 order of operations, secret rotation, and
the local development loop.

`scripts/test/` holds the network-namespace regression harness for the Connection ID rebind
path. It builds a throwaway privileged container with no external network, moves a client
between simulated NAT bindings, and asserts the session survives. `scripts/test/README.md`
has the details.

## Roadmap

The near-term direction is LwM2M: the transport, security, and block-wise layers here are the
substrate an LwM2M server needs, and the object and registration layers sit above them.

## License

AGPL-3.0. See `LICENSE`.
