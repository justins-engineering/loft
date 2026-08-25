# loft -- PidgeIoT CoAP terminator. Build from the REPO ROOT context
# (cargo needs the whole workspace's manifests to resolve `-p loft`):
#
#   docker build -f loft/Dockerfile -t loft .
#
# See docs/infra/coap-terminator.md for deployment.

FROM rust:1-trixie AS build
# libmbedtls-dev: the mbedTLS-backed DTLS listener (RFC 9146 CID) links the
# system library; the shim's compile-time #error probes gate the build
# headers' feature set here.
RUN apt-get update \
  && apt-get install -y --no-install-recommends libmbedtls-dev \
  && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
# Every shipped binary is dual-stack (LOFT_DTLS_STACK selects at runtime);
# the mbedtls cargo feature exists only so dev machines without the
# library can still build the OpenSSL-only default. psk_stub is the
# runtime stage's handshake check fixture, built here because its crate
# links the same system mbedTLS.
RUN cargo build -p loft --release --features mbedtls \
  && cargo build --release --manifest-path scripts/test/cid-harness/Cargo.toml --bin psk_stub

FROM debian:trixie-slim
RUN apt-get update \
  && apt-get install -y --no-install-recommends libssl3t64 libmbedtls21 ca-certificates binutils \
  && rm -rf /var/lib/apt/lists/* \
  # Sanity-check at image build time that the runtime mbedTLS shared
  # library exports Connection ID support: the shim's #error probes only
  # cover the build stage's headers, and a CID-less runtime .so would fail
  # every rebind silently. binutils is here only for this nm and leaves in
  # the same layer.
  && nm -D /usr/lib/*/libmbedtls.so.21 | grep -qw mbedtls_ssl_conf_cid \
  && apt-get purge -y --auto-remove binutils \
  && useradd --system --no-create-home loft
COPY --from=build /src/target/release/loft /usr/local/bin/loft
# Prove the PSK suites are SELECTED by this image's OpenSSL, not merely
# listed: OpenSSL's default security level refuses PSK-AES128-CCM8 while
# `openssl ciphers` still prints it, so a grep over that output passes on
# a library that fails every CCM8-only device. Six real handshakes against
# the binary above instead, on both transports and both DTLS stacks. The
# fixtures are bind-mounted so nothing of the check persists in the image;
# the openssl CLI is already here as a dependency of ca-certificates.
RUN --mount=type=bind,from=build,source=/src/scripts/test/cid-harness/target/release/psk_stub,target=/usr/local/bin/psk_stub \
    --mount=type=bind,source=scripts/test/psk-suite-check.sh,target=/usr/local/bin/psk-suite-check.sh \
    psk-suite-check.sh /usr/local/bin/loft /usr/local/bin/psk_stub openssl \
  && psk-suite-check.sh /usr/local/bin/loft /usr/local/bin/psk_stub mbedtls
USER loft
# coaps (DTLS/UDP) + coaps+tcp (TLS/TCP), one registered port.
EXPOSE 5684/udp 5684/tcp
ENTRYPOINT ["/usr/local/bin/loft"]
