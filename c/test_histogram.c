#include "histogram.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    atlas_histogram h, before, part, all;
    atlas_bounds b = {11, 12};
    uint64_t n;
    unsigned i;
    atlas_init(&h);
    assert(atlas_quantile(&h, 5000, &b) == ATLAS_EMPTY && b.lower == 11);
    assert(atlas_quantile(&h, 10001, &b) == ATLAS_INVALID);
    for (i = 0; i < 64; ++i) {
        atlas_init(&h);
        n = UINT64_C(1) << i;
        assert(atlas_record(&h, n, 1) == ATLAS_OK && h.buckets[i+1] == 1);
        assert(atlas_quantile(&h, 9900, &b) == ATLAS_OK && b.lower == n && b.upper == n);
    }
    atlas_init(&h);
    assert(atlas_record(&h, 7, UINT64_MAX) == ATLAS_OK);
    before = h;
    assert(atlas_record(&h, 1, 1) == ATLAS_OVERFLOW);
    assert(memcmp(&h, &before, sizeof h) == 0);
    assert(atlas_merge(&h, &before) == ATLAS_OVERFLOW);
    assert(memcmp(&h, &before, sizeof h) == 0);
    assert(atlas_quantile(&h, 9999, &b) == ATLAS_OK && b.lower == 7 && b.upper == 7);
    atlas_init(&h); atlas_init(&part); atlas_init(&all);
    for (n = 0; n < 10000; ++n) {
        assert(atlas_record(&all, n, 1) == ATLAS_OK);
        assert(atlas_record(n % 2 == 0 ? &h : &part, n, 1) == ATLAS_OK);
    }
    assert(atlas_merge(&h, &part) == ATLAS_OK);
    assert(memcmp(&h, &all, sizeof h) == 0);
    atlas_init(&h);
    assert(atlas_record(&h, 0, 1) == ATLAS_OK);
    assert(atlas_record(&h, UINT64_MAX, 1) == ATLAS_OK);
    assert(atlas_quantile(&h, 5000, &b) == ATLAS_OK && b.upper == 0);
    assert(atlas_quantile(&h, 10000, &b) == ATLAS_OK && b.upper == UINT64_MAX);
    assert(atlas_merge(&h, &h) == ATLAS_OK && h.count == 4 && h.buckets[64] == 2);
    puts("C histogram: boundary, overflow, merge and endpoint checks passed");
    return 0;
}
