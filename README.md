# Latency Atlas

**Know the bounds of your tail latency.** Streaming, mergeable base-2 histograms in Rust and C, with an independent Python conformance harness.

Rust · C11 · Python · No third-party runtime dependencies

```text
LATENCY ATLAS
8 samples · nanoseconds
Percentiles are inclusive bucket bounds.
p50  4 .. 7 ns
p95  64 .. 100 ns
p99  64 .. 100 ns
```

The example uses `0, 1, 2, 4, 8, 16, 32, 100`. These are deliberately coarse buckets. A result such as `[64,100]` is a range containing the exact order statistic, not an exact p99 of 100 or an estimate with 1% error.

## Why this project exists

Collecting every request duration is simple until memory grows with traffic. Latency Atlas keeps **65 counters plus count/min/max**, independent of input size. It makes the precision tradeoff explicit, rejects counter overflow without partial mutation, and checks two implementations against exact ranks rather than merely comparing them to each other.

This is an educational systems tool for integer nanosecond samples. It does not instrument an application, correct coordinated omission, aggregate rolling windows, or offer HDRHistogram-level precision. No production adoption or performance superiority is claimed.

## Build and use

Requires stable Rust/Cargo. The Rust package has no external crates.

```sh
cargo test
cargo build --release
printf '0\n1\n2\n4\n8\n16\n32\n100\n' | target/release/latency-atlas
printf '10\n20\n30\n' | target/release/latency-atlas --json
```

PowerShell equivalent:

```powershell
0,1,2,4,8,16,32,100 | .\target\release\latency-atlas.exe
```

Input is one unsigned decimal `u64` per line. ASCII whitespace and empty lines are accepted. Negative values, signs, fractions, NUL bytes, invalid UTF-8, values beyond `u64`, and lines longer than 64 bytes are rejected. The 64-byte limit includes line endings. Errors exit with code 2 and emit no partial report. Samples are processed one at a time; input is never loaded as a complete file.

## C implementation

The portable C11 library has no allocation and no global state. A caller owns and initializes each `atlas_histogram`. Concurrent updates to the same object require caller synchronization. Public struct fields are an ABI choice; mutating them directly violates the library contract.

```sh
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 c/histogram.c c/test_histogram.c -o build/test_histogram
./build/test_histogram
cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 c/histogram.c c/main.c -o build/latency-atlas-c
python tests/conformance.py --rust target/release/latency-atlas --c build/latency-atlas-c
```

On Windows with GCC, use `New-Item -ItemType Directory -Force build`, add `.exe` to output paths, and use `./build/test_histogram.exe`. The C CLI currently emits JSON only. The library also supports weighted observations and histogram merges, which are tested directly.

## Contract

| Property | Definition |
| --- | --- |
| Bucket 0 | `[0,0]` |
| Bucket i, 1–63 | `[2^(i-1), 2^i-1]` |
| Bucket 64 | `[2^63, u64::MAX]` |
| Rank | `ceil(count * basis_points / 10000)` |
| p0 / p100 | Exact observed min / max |
| Empty histogram | No quantile; JSON `null` |
| Bound tightening | Intersect selected bucket with observed min/max |
| Overflow | Return error and preserve original histogram |
| Merge | Same bucket scheme, sum counters, min of minima, max of maxima |

Rust uses `u128` for rank arithmetic. C splits the multiplication into quotient and remainder to avoid overflow without compiler-specific 128-bit integers. No floating-point calculation enters the histogram implementation. Memory layout differs by language; binary struct serialization is deliberately not supported.

## Evidence and review guide

- Rust tests exercise every power-of-two boundary, exact-rank containment over 10,000 deterministic observations, partition/merge equivalence, zero weight, endpoint handling, and atomic overflow.
- C tests cover matching boundary and overflow invariants, partition merging, and self-merge.
- The black-box harness feeds 38 distributions into both CLIs, checks JSON equality, and independently verifies that exact sorted percentiles fall within returned bounds. It also checks malformed inputs and CRLF.
- CI runs formatting, Clippy with warnings denied, Rust tests, strict C warnings, AddressSanitizer/UndefinedBehaviorSanitizer on the C tests, and cross-language checks.

[Numerical design and tradeoffs](docs/DESIGN.md) · [Reproducible benchmarking](docs/BENCHMARKS.md) · [Contributing](CONTRIBUTING.md)

## Provenance

Created for Ismaeel Habeeb's public engineering portfolio with AI-assisted implementation and documentation. This is a new reference project, not evidence of years of production operation. Its value is in the contract, inspectable algorithms, and reproducible checks.
