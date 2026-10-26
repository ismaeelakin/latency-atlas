#include "histogram.h"
#include <stddef.h>

void atlas_init(atlas_histogram *h) {
    size_t i;
    for (i = 0; i < 65; ++i) h->buckets[i] = 0;
    h->count = 0; h->min = UINT64_MAX; h->max = 0;
}

int atlas_record(atlas_histogram *h, uint64_t value, uint64_t weight) {
    unsigned index = 0;
    uint64_t n = value;
    if (weight == 0) return ATLAS_OK;
    if (UINT64_MAX - h->count < weight) return ATLAS_OVERFLOW;
    while (n != 0) { ++index; n >>= 1; }
    h->buckets[index] += weight;
    h->count += weight;
    if (value < h->min) h->min = value;
    if (value > h->max) h->max = value;
    return ATLAS_OK;
}

int atlas_merge(atlas_histogram *h, const atlas_histogram *other) {
    size_t i;
    uint64_t count, min, max;
    if (UINT64_MAX - h->count < other->count) return ATLAS_OVERFLOW;
    count = h->count + other->count;
    min = h->min < other->min ? h->min : other->min;
    max = h->max > other->max ? h->max : other->max;
    /* Cache metadata first: self-merge is supported. */
    for (i = 0; i < 65; ++i) h->buckets[i] += other->buckets[i];
    h->count = count; h->min = min; h->max = max;
    return ATLAS_OK;
}

int atlas_quantile(const atlas_histogram *h, unsigned p, atlas_bounds *out) {
    uint64_t rank, cumulative = 0;
    unsigned i;
    atlas_bounds bounds;
    if (p > 10000) return ATLAS_INVALID;
    if (h->count == 0) return ATLAS_EMPTY;
    if (p == 0 || p == 10000) {
        out->lower = out->upper = p == 0 ? h->min : h->max;
        return ATLAS_OK;
    }
    /* ceil(count*p/10000), without multiplication overflow or floating point. */
    rank = (h->count / 10000) * p + ((h->count % 10000) * p + 9999) / 10000;
    for (i = 0; i < 65; ++i) {
        cumulative += h->buckets[i];
        if (cumulative >= rank) {
            bounds.lower = i == 0 ? 0 : UINT64_C(1) << (i - 1);
            bounds.upper = i == 64 ? UINT64_MAX : (UINT64_C(1) << i) - 1;
            if (bounds.lower < h->min) bounds.lower = h->min;
            if (bounds.upper > h->max) bounds.upper = h->max;
            *out = bounds;
            return ATLAS_OK;
        }
    }
    return ATLAS_INVALID;
}
