# Wire-format goldens

Each `*.hex` file contains the byte-exact hex output of the legacy Python tool
encoding a specific message. Rust integration tests in `../golden_*.rs`
re-encode the same message in Rust and assert equality.

To regenerate (e.g. after legacy Python code changes):

    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_goldens.py

If the Rust encode diverges from the goldens, fix one side until they match.
Drift is almost always a bug somewhere.
