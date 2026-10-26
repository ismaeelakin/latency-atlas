# Numerical design

## Why base 2

Bucket selection is the bit width of the observation: zero maps to bucket zero; other values map to `floor(log2(value))+1`. Rust uses `leading_zeros`; C uses a portable shift loop. Ingesting n observations takes O(n) time, with a constant-size histogram. Selecting one quantile scans at most 65 counters; merge visits 65 counters. The CLI's read buffer is bounded separately from histogram storage.

This intentionally trades precision for simplicity. A large bucket can span nearly a factor of two. The API returns inclusive lower/upper bounds so downstream users cannot confuse the output with an exact percentile. Bounds are clamped to observed min/max. For example, observations [8,9] need not return an upper bound of 15.

## Rank arithmetic

Basis points are integer values 0–10000. For a nonempty histogram, p0 returns the exact minimum and p100 returns the exact maximum. All interior quantiles use nearest rank, starting from rank 1.

`ceil(n*p/10000)` can overflow a u64 even when the result fits. Rust promotes to u128. C writes `n=10000*q+r`, and computes `q*p + ceil(r*p/10000)`. With `p<=10000` and `r<10000`, each intermediate and the final rank fit in u64. Weighted counts use the same semantics as repeated observations without the repeated processing cost.

## Mutation invariants

The sum of all buckets always equals count; min/max summarize all observations when count is nonzero. Checked total-count addition happens before record or merge mutates any field. Since each nonnegative bucket is at most the total, this total check also proves each bucket addition safe. A zero-weight record leaves even min/max untouched.

Rust fields are private. C exposes its struct for predictable ownership and interop, so valid initialization and no direct field mutation are preconditions. This project avoids unsafe Rust and cross-language FFI: the independent implementations share a behavioral contract and CLI schema, not process memory.

## Merge properties and limitations

Compatible histogram merge is associative and commutative when total counts fit. Partitioning a stream and merging its histograms must produce the same counts, extrema, and quantile bounds as a single pass. Rust needs a clone to self-merge; C explicitly supports aliasing the source and destination.

There is no subtraction API: removing old windows requires retaining a ring of separate window histograms, and deciding when a window closes is a separate distributed-systems problem. Persisted binary formats, timestamp units other than nanoseconds, invalid-structure recovery, concurrent mutation, and high-resolution sub-buckets are outside v0.1.

## Tests as an oracle

Rust/C agreement alone can hide a shared mistake. The Python harness also sorts the original samples, selects the exact nearest-rank observation, and checks containment. Boundary fixtures include zero, u64 maximum, powers of two and neighbors, a single outlier, all-identical values, and deterministic random distributions.
