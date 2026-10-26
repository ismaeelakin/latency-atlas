# Contributing

Run `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test`, the strict C compile commands in README, and `tests/conformance.py` before proposing changes.

Keep Rust and C behavior consistent with the numerical contract. For arithmetic changes, include values near zero, powers of two, and u64 limits. Expand the independent rank oracle rather than testing only implementation equality. Avoid adding dependencies or changing percentile semantics without describing the tradeoff.
