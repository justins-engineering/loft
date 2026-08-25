#!/usr/bin/env bash
# Proves the PSK suites loft pins are SELECTED by the OpenSSL it runs on,
# not merely listed. A cipher list can carry a suite the library never
# chooses: OpenSSL's default security level rates PSK-AES128-CCM8 below
# its floor, and `openssl ciphers` prints the suite identically at every
# level, so only a handshake can answer the question. This completes six
# of them with `openssl s_client` against a running loft, on both
# transports:
#
#   CCM8 alone            -> must negotiate CCM8
#   GCM alone             -> must negotiate GCM
#   CCM8 offered over GCM -> must negotiate CCM8 (GCM here would mean CCM8
#                            was excluded and the client fell through)
#
# Usage:
#   psk-suite-check.sh <loft binary> <psk_stub binary> [openssl|mbedtls]
#
# Starts loft on 127.0.0.1:${PSK_CHECK_PORT:-5684} (both listeners, the
# named DTLS stack) against psk_stub on 127.0.0.1:${PSK_CHECK_STUB_PORT:-8788},
# runs the matrix, and exits non-zero on any mismatch or on a handshake
# count in loft's own journal that disagrees with the client. The stub's
# identity and key are harness-minted, not credentials. Run by
# loft/Dockerfile at image build time and by the netns harness.
set -uo pipefail

LOFT=${1:?loft binary}
STUB=${2:?psk_stub binary}
STACK=${3:-openssl}
PORT=${PSK_CHECK_PORT:-5684}
STUB_PORT=${PSK_CHECK_STUB_PORT:-8788}
# The one credential psk_stub answers for (scripts/test/cid-harness).
IDENTITY=cid-harness-pigeon
SECRET=0123456789abcdef0123456789abcdef
# `od -v`: without it od folds the secret's repeated half into a "*" line.
HEX=$(printf '%s' "$SECRET" | od -An -v -tx1 | tr -d ' \n')

command -v openssl >/dev/null || { echo "psk-suite-check: openssl CLI not found"; exit 2; }

WORK=$(mktemp -d)
LOFT_LOG="$WORK/loft.log"
STUB_PID=
LOFT_PID=
cleanup() {
  [ -n "$LOFT_PID" ] && kill "$LOFT_PID" 2>/dev/null
  [ -n "$STUB_PID" ] && kill "$STUB_PID" 2>/dev/null
  wait 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT

"$STUB" "127.0.0.1:$STUB_PORT" >"$WORK/stub.log" 2>&1 &
STUB_PID=$!
sleep 0.3
if ! kill -0 "$STUB_PID" 2>/dev/null; then
  echo "psk-suite-check: psk_stub did not start (port $STUB_PORT busy?):"
  sed 's/^/  /' "$WORK/stub.log"
  exit 1
fi
COAP_SERVICE_SECRET=psk-suite-check-not-a-real-secret \
  LOFT_DTLS_STACK="$STACK" \
  LOFT_UDP_LISTEN="127.0.0.1:$PORT" \
  LOFT_TCP_LISTEN="127.0.0.1:$PORT" \
  LOFT_DOVECOTE_URL="http://127.0.0.1:$STUB_PORT" \
  LOFT_LOG=info \
  "$LOFT" >"$LOFT_LOG" 2>&1 &
LOFT_PID=$!

# Both listeners announce themselves; a probe before that would read as a
# negotiation failure.
deadline=$((SECONDS + 15))
until grep -q 'DTLS/UDP listener up' "$LOFT_LOG" && grep -q 'TLS/TCP listener up' "$LOFT_LOG"; do
  if ! kill -0 "$LOFT_PID" 2>/dev/null || [ "$SECONDS" -ge "$deadline" ]; then
    echo "psk-suite-check: loft ($STACK) did not bring both listeners up:"
    sed 's/^/  /' "$LOFT_LOG"
    exit 1
  fi
  sleep 0.1
done

# The suite s_client reports, or NONE. s_client names the suite as soon
# as the ServerHello carries one, even if the handshake then fails, so
# this line never carries a verdict on its own. The TCP listener's CSM
# frame lands on stdout as binary, hence the NUL strip.
negotiate() {
  local proto="$1" offer="$2"
  timeout 30 openssl s_client "$proto" -connect "127.0.0.1:$PORT" \
      -psk_identity "$IDENTITY" -psk "$HEX" \
      -cipher "$offer:@SECLEVEL=0" -ciphersuites '' </dev/null 2>&1 \
    | tr -d '\0' | sed -n 's/^New, [^,]*, Cipher is //p' | head -1
}
established() { grep -c 'session established' "$LOFT_LOG"; }

FAILURES=0
for proto in -dtls1_2 -tls1_2; do
  for case in \
    "PSK-AES128-CCM8|PSK-AES128-CCM8" \
    "PSK-AES128-GCM-SHA256|PSK-AES128-GCM-SHA256" \
    "PSK-AES128-CCM8:PSK-AES128-GCM-SHA256|PSK-AES128-CCM8"
  do
    offer=${case%%|*}
    want=${case##*|}
    before=$(established)
    got=$(negotiate "$proto" "$offer")
    # A PASS needs both accounts: the client saw the wanted suite AND loft
    # logged exactly one more established session for it.
    sleep 0.2
    after=$(established)
    if [ "$got" = "$want" ] && [ "$after" -eq $((before + 1)) ]; then
      printf 'PASS %-8s %-7s offer=%-40s -> %s\n' "$STACK" "$proto" "$offer" "$got"
    else
      printf 'FAIL %-8s %-7s offer=%-40s -> %s (want %s; sessions established %d->%d)\n' \
        "$STACK" "$proto" "$offer" "${got:-NONE}" "$want" "$before" "$after"
      FAILURES=$((FAILURES + 1))
    fi
  done
done

if [ "$FAILURES" -ne 0 ]; then
  echo "psk-suite-check ($STACK): $FAILURES FAILED; loft journal:"
  sed 's/^/  /' "$LOFT_LOG"
  exit 1
fi
echo "psk-suite-check ($STACK): all suites selected"
