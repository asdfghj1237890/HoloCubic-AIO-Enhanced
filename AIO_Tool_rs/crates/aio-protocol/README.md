# aio-protocol

HoloCubic AIO wire protocol — encode and decode messages exchanged with
HoloCubic firmware over USB serial or TCP.

## Design notes

- `MsgHead` is a 7-byte fixed header with magic `0x2323`.
- **Asymmetric byte order**: encode is little-endian (Python `=` default),
  decode is big-endian (matches firmware encode and Python `!` default).
  See plan 1 `Discovery & Constraints` D1.
- File / dir messages have an 8-byte header: 7-byte MsgHead + a duplicate
  `action_type` byte. See plan 1 D2.
- Several v2.x bugs are preserved byte-for-byte for wire compatibility with
  existing firmware — see `BUGS.md`.

## Testing

```sh
cargo test -p aio-protocol
```

Three layers:

- Unit tests in `src/*.rs` cover individual struct logic and enum stability.
- Property tests (`proptest`) cover SettingMsg encode/decode round-trip.
- Golden snapshots in `tests/golden/*.hex` are byte-exact captures of the
  legacy Python encoder; Rust must match. Regenerate with:

  ```sh
  cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_goldens.py
  ```

## Stability

This crate's surface is FROZEN at the wire format. Any change to encode
output, decode parsing, or enum discriminants needs a deliberate audit —
see PR template `## Protocol-Surface` checklist.
