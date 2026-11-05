# Rust C API

The Rust histogram can be used from C through `c/rust_histogram.h`. Its handles are opaque, so C code cannot depend on or alter Rust's internal layout. The native C implementation remains available under its existing names.

## Build and link

Build the shared library with Cargo, then link the C caller against it. These commands use a Linux GCC-compatible toolchain:

```sh
cargo build --release
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 \
  -Ic c/histogram.c c/ffi_conformance.c \
  -Ltarget/release -llatency_atlas \
  -Wl,-rpath,'$ORIGIN/../target/release' \
  -o build/ffi_conformance
printf '0\n1\n2\n4\n8\n16\n32\n100\n' | build/ffi_conformance
```

The C program calls the Rust library and the native C implementation with the same input. It prints the Rust results only after both implementations agree. The Python conformance harness checks those results against exact sorted ranks.

On Windows, use MSYS2 GCC with the `x86_64-pc-windows-gnu` Rust target. Link against the import library in `target/release`, then add that directory to `PATH` before running the C executable so Windows can find the Rust DLL.

## Ownership and errors

`atlas_rust_histogram_create` returns a handle owned by the caller. Release it once with `atlas_rust_histogram_destroy`; destroying `NULL` is safe. Every other function returns `ATLAS_RUST_INVALID_ARGUMENT` for a null handle or required output pointer. A non-null handle must have come from `create`, remain alive for the call, and not be destroyed twice. Output pointers must refer to writable storage separate from the handle. The API cannot detect a fabricated or dangling pointer.

Each handle may be used by one thread at a time. Callers must synchronize concurrent access. `merge` accepts the same handle as both arguments and doubles its contents if the count does not overflow. Overflow leaves the histogram unchanged. The `min`, `max`, and `quantile` outputs are left unchanged when the result is empty or the call fails.

| Status | Meaning |
| --- | --- |
| `ATLAS_RUST_OK` | The operation completed. |
| `ATLAS_RUST_OVERFLOW` | A record or merge would exceed the `u64` count. |
| `ATLAS_RUST_EMPTY` | A minimum, maximum, or quantile was requested from an empty histogram. |
| `ATLAS_RUST_INVALID_ARGUMENT` | A required pointer is null or the percentile exceeds 10000 basis points. |

The C ABI does not permit Rust panics to unwind into C. An unexpected panic aborts the process. The FFI wrapper does not catch arbitrary pointer misuse; C callers must honor the handle and output-pointer requirements above.
