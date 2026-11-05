use std::ffi::c_int;

use crate::{Bounds, Error, Histogram};

const OK: c_int = 0;
const OVERFLOW: c_int = 1;
const EMPTY: c_int = 2;
const INVALID_ARGUMENT: c_int = 3;

#[repr(C)]
pub struct AtlasRustBounds {
    pub lower: u64,
    pub upper: u64,
}

#[repr(C)]
pub struct AtlasRustHistogram {
    // C sees only a pointer to this type; it must not inspect the field.
    inner: Histogram,
}

fn error_code(error: Error) -> c_int {
    match error {
        Error::Overflow => OVERFLOW,
        Error::InvalidPercentile => INVALID_ARGUMENT,
    }
}

#[unsafe(no_mangle)]
/// Creates an empty histogram. The returned handle belongs to the caller.
pub extern "C" fn atlas_rust_histogram_create() -> *mut AtlasRustHistogram {
    Box::into_raw(Box::new(AtlasRustHistogram {
        inner: Histogram::default(),
    }))
}

/// Releases a handle returned by `atlas_rust_histogram_create`.
///
/// # Safety
/// A non-null handle must be live, must have come from `create`, and may be
/// destroyed only once. Null is accepted as a no-op.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_destroy(histogram: *mut AtlasRustHistogram) {
    if !histogram.is_null() {
        // SAFETY: The caller must pass a live handle returned by create and may
        // destroy it only once. Null is accepted as a no-op.
        drop(unsafe { Box::from_raw(histogram) });
    }
}

/// Records one observation.
///
/// # Safety
/// The handle must be live and must not be used concurrently during this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_record(
    histogram: *mut AtlasRustHistogram,
    value: u64,
) -> c_int {
    if histogram.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle returned by create.
    let histogram = unsafe { &mut *histogram };
    histogram
        .inner
        .record(value)
        .map_or_else(error_code, |_| OK)
}

/// Records an observation with a weight. A zero weight is a no-op.
///
/// # Safety
/// The handle must be live and must not be used concurrently during this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_record_n(
    histogram: *mut AtlasRustHistogram,
    value: u64,
    weight: u64,
) -> c_int {
    if histogram.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle returned by create.
    let histogram = unsafe { &mut *histogram };
    histogram
        .inner
        .record_n(value, weight)
        .map_or_else(error_code, |_| OK)
}

/// Merges `source` into `destination`. Self-merge is supported.
///
/// # Safety
/// Both handles must be live, and neither may be used concurrently during this
/// call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_merge(
    destination: *mut AtlasRustHistogram,
    source: *const AtlasRustHistogram,
) -> c_int {
    if destination.is_null() || source.is_null() {
        return INVALID_ARGUMENT;
    }

    if std::ptr::eq(destination.cast_const(), source) {
        // SAFETY: The caller must pass a live handle returned by create. The
        // source is copied before mutation, so self-merge does not alias a Rust
        // shared reference with the mutable destination.
        let destination = unsafe { &mut *destination };
        let source = destination.inner.clone();
        return destination
            .inner
            .merge(&source)
            .map_or_else(error_code, |_| OK);
    }

    // SAFETY: Both pointers must refer to distinct live handles returned by
    // create. The pointer comparison above rules out aliasing.
    let destination = unsafe { &mut *destination };
    let source = unsafe { &*source };
    destination
        .inner
        .merge(&source.inner)
        .map_or_else(error_code, |_| OK)
}

/// Writes the current observation count.
///
/// # Safety
/// The handle must be live. `count` must point to writable `u64` storage that
/// does not overlap the handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_count(
    histogram: *const AtlasRustHistogram,
    count: *mut u64,
) -> c_int {
    if histogram.is_null() || count.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle and writable output storage.
    let histogram = unsafe { &*histogram };
    unsafe { *count = histogram.inner.count() };
    OK
}

/// Writes the observed minimum, or returns `EMPTY` without changing `value`.
///
/// # Safety
/// The handle must be live. `value` must point to writable `u64` storage that
/// does not overlap the handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_min(
    histogram: *const AtlasRustHistogram,
    value: *mut u64,
) -> c_int {
    if histogram.is_null() || value.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle and writable output storage.
    let histogram = unsafe { &*histogram };
    match histogram.inner.min() {
        Some(min) => {
            unsafe { *value = min };
            OK
        }
        None => EMPTY,
    }
}

/// Writes the observed maximum, or returns `EMPTY` without changing `value`.
///
/// # Safety
/// The handle must be live. `value` must point to writable `u64` storage that
/// does not overlap the handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_max(
    histogram: *const AtlasRustHistogram,
    value: *mut u64,
) -> c_int {
    if histogram.is_null() || value.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle and writable output storage.
    let histogram = unsafe { &*histogram };
    match histogram.inner.max() {
        Some(max) => {
            unsafe { *value = max };
            OK
        }
        None => EMPTY,
    }
}

/// Writes percentile bounds for an integer basis-point percentile.
///
/// # Safety
/// The handle must be live. `bounds` must point to writable storage that does
/// not overlap the handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn atlas_rust_histogram_quantile(
    histogram: *const AtlasRustHistogram,
    basis_points: u16,
    bounds: *mut AtlasRustBounds,
) -> c_int {
    if histogram.is_null() || bounds.is_null() {
        return INVALID_ARGUMENT;
    }

    // SAFETY: The caller must pass a live handle and writable output storage.
    let histogram = unsafe { &*histogram };
    match histogram.inner.quantile(basis_points) {
        Ok(Some(Bounds { lower, upper })) => {
            unsafe { *bounds = AtlasRustBounds { lower, upper } };
            OK
        }
        Ok(None) => EMPTY,
        Err(error) => error_code(error),
    }
}
