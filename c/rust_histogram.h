#ifndef ATLAS_RUST_HISTOGRAM_H
#define ATLAS_RUST_HISTOGRAM_H

#include <stdint.h>

typedef struct AtlasRustHistogram AtlasRustHistogram;

typedef struct {
    uint64_t lower;
    uint64_t upper;
} atlas_rust_bounds;

enum {
    ATLAS_RUST_OK = 0,
    ATLAS_RUST_OVERFLOW = 1,
    ATLAS_RUST_EMPTY = 2,
    ATLAS_RUST_INVALID_ARGUMENT = 3
};

/* Create returns an owned handle; destroy releases it. Destroying NULL is safe.
 * Every other call rejects NULL. A non-NULL handle must be live, created here,
 * and used by one thread at a time unless the caller synchronizes access.
 * Output pointers must refer to writable storage separate from the handle.
 * Handles cannot be checked for arbitrary invalid or dangling pointers.
 */
AtlasRustHistogram *atlas_rust_histogram_create(void);
void atlas_rust_histogram_destroy(AtlasRustHistogram *histogram);

int atlas_rust_histogram_record(AtlasRustHistogram *histogram, uint64_t value);
int atlas_rust_histogram_record_n(AtlasRustHistogram *histogram, uint64_t value,
                                  uint64_t weight);
/* Self-merge is supported and doubles the stored count when it does not overflow. */
int atlas_rust_histogram_merge(AtlasRustHistogram *destination,
                              const AtlasRustHistogram *source);

int atlas_rust_histogram_count(const AtlasRustHistogram *histogram,
                               uint64_t *count);
int atlas_rust_histogram_min(const AtlasRustHistogram *histogram, uint64_t *value);
int atlas_rust_histogram_max(const AtlasRustHistogram *histogram, uint64_t *value);
/* basis_points is 0..=10000. Output is written only on success. */
int atlas_rust_histogram_quantile(const AtlasRustHistogram *histogram,
                                  uint16_t basis_points,
                                  atlas_rust_bounds *bounds);

#endif
