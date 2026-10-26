#ifndef ATLAS_HISTOGRAM_H
#define ATLAS_HISTOGRAM_H
#include <stdint.h>

/* Initialize with atlas_init. Fields are public for C ABI portability; callers
 * must not mutate them. Operations require valid, initialized, non-null objects.
 * No allocation; no global state. One histogram must not be shared unsynchronized.
 */
typedef struct {
    uint64_t buckets[65];
    uint64_t count;
    uint64_t min;
    uint64_t max;
} atlas_histogram;

typedef struct { uint64_t lower, upper; } atlas_bounds;
enum { ATLAS_OK = 0, ATLAS_OVERFLOW = 1, ATLAS_EMPTY = 2, ATLAS_INVALID = 3 };

void atlas_init(atlas_histogram *h);
int atlas_record(atlas_histogram *h, uint64_t value, uint64_t weight);
int atlas_merge(atlas_histogram *h, const atlas_histogram *other);
/* basis_points is 0..10000. Errors leave output unchanged. */
int atlas_quantile(const atlas_histogram *h, unsigned basis_points, atlas_bounds *out);
#endif
