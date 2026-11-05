"""Black-box Rust/C checks against an independent exact-order-statistic oracle."""
import argparse
import json
import math
from pathlib import Path
import random
import subprocess


def run(binary, content):
    return subprocess.run([str(Path(binary).resolve()), "--json"], input=content,
                          capture_output=True, timeout=10)


def run_ffi(binary, content):
    return subprocess.run([str(Path(binary).resolve())], input=content,
                          capture_output=True, timeout=10)


def check_exact_bounds(result, values):
    assert result["count"] == len(values)
    assert result["min"] == (min(values) if values else None)
    assert result["max"] == (max(values) if values else None)
    ordered = sorted(values)
    for name, percentile in [("p50", 50), ("p95", 95), ("p99", 99)]:
        bounds = result["quantiles"][name]
        if not values:
            assert bounds is None
        else:
            exact = ordered[math.ceil(len(values) * percentile / 100) - 1]
            assert bounds[0] <= exact <= bounds[1], (name, exact, bounds)


def verify(rust, c, rust_ffi=None):
    rng = random.Random(20261026)
    fixtures = [[], [0], [1], [2**64 - 1], [0, 2**64 - 1],
                list(range(1001)), [1] * 999 + [1000000],
                [max(0, (1 << bit) + delta) for bit in range(64) for delta in (-1, 0, 1)
                 if (1 << bit) + delta < 2**64]]
    fixtures += [[rng.randrange(2**rng.randrange(1, 65)) for _ in range(rng.randrange(1, 3000))]
                 for _ in range(30)]
    for values in fixtures:
        data = ("\n".join(map(str, values)) + "\n").encode()
        outputs = [run(binary, data) for binary in (rust, c)]
        assert all(p.returncode == 0 for p in outputs), outputs
        results = [json.loads(p.stdout) for p in outputs]
        assert results[0] == results[1], results
        check_exact_bounds(results[0], values)
        if rust_ffi:
            ffi = run_ffi(rust_ffi, data)
            assert ffi.returncode == 0, (values, ffi.stderr.decode(errors="replace"))
            ffi_result = json.loads(ffi.stdout)
            assert ffi_result == results[0], (ffi_result, results[0])
            check_exact_bounds(ffi_result, values)
    invalid = [b"-1\n", b"+2\n", b"1.2\n", b"18446744073709551616\n",
               b"1\nhello\n", b"9" * 65 + b"\n", b"\xff\n", b"1\x00junk\n", "\u20031\n".encode()]
    for data in invalid:
        for binary in (rust, c):
            result = run(binary, data)
            assert result.returncode == 2 and not result.stdout, (binary, data, result)
    for binary in (rust, c):
        assert json.loads(run(binary, b" \r\n 0 \r\n42\r\n").stdout)["count"] == 2
    extra = "; Rust C API = native C" if rust_ffi else ""
    print(
        f"PASS: {len(fixtures)} valid distributions, {len(invalid)} invalid inputs, "
        f"whitespace/CRLF; Rust = C; exact ranks inside bounds{extra}"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--rust", required=True)
    parser.add_argument("--c", required=True)
    parser.add_argument("--rust-ffi")
    args = parser.parse_args()
    verify(args.rust, args.c, args.rust_ffi)
