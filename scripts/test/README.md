# loft CID test harness (netns Tier 2)

The RFC 9146 Connection ID rollout (`docs/infra/coap-cid-design.md`) has a
three-tier test plan. Tiers 1 and shim-level coverage live in the crates
themselves:

- `cargo test -p mbedtls-ffi-shim` (run in the trixie build image) — the
  shim's CID loopback, HelloVerifyRequest flow, garbage corpus,
  zero-allocation guard, timer-driven retransmission, and PSK-reject
  indistinguishability.
- `cargo test -p loft --features mbedtls` (same image) — the listener's
  demux, including a socket-swap rebind, replay drop, no-CID regression,
  and the address-reuse re-handshake.

This directory is **Tier 2**: the same properties proven against the *real
`loft` binary* through a *real NAT*, on the wire, rather than an in-process
socket swap. It is the acceptance gate for the feature itself.

## Running

```sh
scripts/test/cid-netns-harness.sh            # build the image + run
scripts/test/cid-netns-harness.sh --no-build # reuse an existing image
```

It builds `loft-cid-harness` (a trixie image carrying the real
`--features mbedtls` loft binary plus a tiny CID client and a stub PSK
endpoint) and runs it **privileged** — the netns/veth/nftables/conntrack
work needs it. It is a throwaway container on a dev box; nothing here ever
touches the VPS, and the PSKs are harness-minted (no real credentials, no
real dovecote).

## Suite check (runs first)

Before any cell, `psk-suite-check.sh` proves each pinned PSK suite is
actually **selected** by the image's OpenSSL, for both DTLS stacks and both
transports, on the container's loopback. A cipher list can carry a suite
the library never chooses — OpenSSL's default security level lists
`PSK-AES128-CCM8` and refuses to select it, and `openssl ciphers` cannot
tell the difference — so this is six real `openssl s_client` handshakes
against the real binary: CCM8 alone, GCM alone, and CCM8 offered over GCM,
each of which must both report the wanted suite client-side and add
exactly one `session established` line to loft's journal (s_client names a
suite as soon as the ServerHello carries one, even if the handshake then
fails, so the client line alone is not a verdict). The same script is the
gate in `loft/Dockerfile`'s runtime stage.

## Topology

```
 cli (10.1.0.2) ──veth──▶ nat (10.1.0.1 / 10.2.0.1) ──veth──▶ srv (10.2.0.2)
       cid_client            nftables SNAT + conntrack            loft + psk_stub
```

`nat` source-NATs the client to an exact port so a "rebind" is a
deterministic port change: the cell swaps the SNAT rule to a new port (one
atomic `nft -f` transaction, so no datagram leaks un-NAT'd) and flushes
conntrack, which is exactly what a carrier NAT does to a PSM'd device that
slept through its mapping. `loft` runs `LOFT_DTLS_STACK=mbedtls`; the stub
answers `GET /internal/coap-psk/:id` over srv's loopback, so loft's real
resolver path is exercised without a real backend.

## Cells

1. **`cid-rebind-survival`** (primary) — the CID client rebinds mid-run.
   Asserts: all exchanges succeed; the client performs exactly **one**
   handshake, cross-checked against loft's own established-session count
   (so the client's self-report can't carry it alone); type-25 CID records
   appear on both the pre- and **post-rebind** source ports (the
   post-rebind evidence is what proves the session survived, not just that
   CID engaged before); **zero** post-rebind handshake records (no
   re-handshake); every downlink record is content type 23 (the device
   blackholes CID-bearing downlink, so this is load-bearing); and loft's
   journal shows **exactly one** CID-negotiated session and **exactly one**
   address migration.
2. **`no-cid-regression`** — a client offering no CID takes the identical
   rebind. Asserts: it is forced into a **second** handshake to recover
   (established-session count of two, and post-rebind handshake records on
   the wire) and no type-25 record ever appears in either direction.
3. **`cid-rebind-v4-to-v6`** — the rebind is a change of address family
   rather than a NAT flip: loft listens dual-stack (`[::]:5684`), the CID
   client opens over the NATed v4 path and, after three exchanges, moves
   itself to its own v6 address (`--rebind-to`, no NAT involved). Asserts
   everything cell 1 does, with the post-rebind uplink read from the v6
   source, plus: the journal's one migration line reads from the *folded*
   v4 source (`10.2.0.1:40000`, not the `::ffff:` mapped form) to the
   bracketed v6 address, and post-rebind downlink records leave over v6.
4. **`no-cid-v4-to-v6`** — cell 2 across the same family change: the
   no-CID client is forced to re-handshake, and the re-handshake happens
   over v6 through the dual-stack listener.

The topology carries IPv6 on the same veth pairs (`fd00:1::/64` client
side, `fd00:2::/64` server side), routed through `nat` rather than NATed;
`setup_topology` asserts v6 forwarding took and that the client can reach
the server over v6 before any cell depends on it.

Both cells first assert the capture is **non-empty** in each direction and
that every datagram parses with **no leftover record tail**, before any
"== 0" check runs — an empty or garbage capture fails loudly instead of
satisfying a zero-count assertion vacuously.

Wire assertions walk **every** DTLS record in **every** datagram straight
from the raw UDP payload (a `gawk` record walker that knows RFC 9146 puts
the 8-byte CID between the sequence number and the length, so a
fixed-offset parser would miscount a coalesced `[23][25]` datagram) — no
dependence on tshark's own DTLS dissector.

On a failing run the pcaps, journals, and client output are copied to the
host artifacts directory the launcher prints; a passing run cleans up.

Anti-spoof/replay, loss-soak, and parallel-listener behaviour are covered
by the crate unit tests above; adding them here as further cells is a
matter of more `run_cell`-shaped functions in `run-cid-netns.sh`.
