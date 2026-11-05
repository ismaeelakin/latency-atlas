#include "histogram.h"
#include "rust_histogram.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Rust C API: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void check_api_edges(void) {
    AtlasRustHistogram *histogram = atlas_rust_histogram_create();
    AtlasRustHistogram *source = atlas_rust_histogram_create();
    AtlasRustHistogram *full = atlas_rust_histogram_create();
    atlas_rust_bounds bounds = {11, 12};
    uint64_t value = 13;

    require(histogram != NULL && source != NULL && full != NULL, "create returned NULL");
    require(atlas_rust_histogram_record(NULL, 1) == ATLAS_RUST_INVALID_ARGUMENT,
            "record accepted a NULL handle");
    require(atlas_rust_histogram_record_n(NULL, 1, 1) == ATLAS_RUST_INVALID_ARGUMENT,
            "weighted record accepted a NULL handle");
    require(atlas_rust_histogram_record_n(histogram, 1, 0) == ATLAS_RUST_OK,
            "zero weight was not a no-op");
    require(atlas_rust_histogram_count(histogram, &value) == ATLAS_RUST_OK && value == 0,
            "empty count was not zero");

    value = 13;
    require(atlas_rust_histogram_min(histogram, &value) == ATLAS_RUST_EMPTY && value == 13,
            "empty minimum changed its output");
    require(atlas_rust_histogram_max(histogram, &value) == ATLAS_RUST_EMPTY && value == 13,
            "empty maximum changed its output");
    require(atlas_rust_histogram_quantile(histogram, 5000, &bounds) == ATLAS_RUST_EMPTY &&
                bounds.lower == 11 && bounds.upper == 12,
            "empty quantile changed its output");
    require(atlas_rust_histogram_quantile(histogram, 10001, &bounds) ==
                ATLAS_RUST_INVALID_ARGUMENT && bounds.lower == 11 && bounds.upper == 12,
            "invalid percentile changed its output");
    require(atlas_rust_histogram_quantile(histogram, 5000, NULL) ==
                ATLAS_RUST_INVALID_ARGUMENT,
            "quantile accepted a NULL output");
    require(atlas_rust_histogram_count(histogram, NULL) == ATLAS_RUST_INVALID_ARGUMENT,
            "count accepted a NULL output");
    require(atlas_rust_histogram_min(NULL, &value) == ATLAS_RUST_INVALID_ARGUMENT,
            "minimum accepted a NULL handle");
    require(atlas_rust_histogram_max(NULL, &value) == ATLAS_RUST_INVALID_ARGUMENT,
            "maximum accepted a NULL handle");
    require(atlas_rust_histogram_min(histogram, NULL) == ATLAS_RUST_INVALID_ARGUMENT,
            "minimum accepted a NULL output");
    require(atlas_rust_histogram_max(histogram, NULL) == ATLAS_RUST_INVALID_ARGUMENT,
            "maximum accepted a NULL output");
    require(atlas_rust_histogram_quantile(NULL, 5000, &bounds) ==
                ATLAS_RUST_INVALID_ARGUMENT,
            "quantile accepted a NULL handle");
    require(atlas_rust_histogram_merge(NULL, histogram) == ATLAS_RUST_INVALID_ARGUMENT,
            "merge accepted a NULL destination");
    require(atlas_rust_histogram_merge(histogram, NULL) == ATLAS_RUST_INVALID_ARGUMENT,
            "merge accepted a NULL source");

    require(atlas_rust_histogram_record_n(histogram, 7, 2) == ATLAS_RUST_OK,
            "weighted record failed");
    require(atlas_rust_histogram_record(source, 13) == ATLAS_RUST_OK,
            "record to merge source failed");
    require(atlas_rust_histogram_merge(histogram, source) == ATLAS_RUST_OK,
            "merge from a distinct handle failed");
    require(atlas_rust_histogram_count(histogram, &value) == ATLAS_RUST_OK && value == 3,
            "merge did not add the source count");
    require(atlas_rust_histogram_merge(histogram, histogram) == ATLAS_RUST_OK,
            "self-merge failed");
    require(atlas_rust_histogram_count(histogram, &value) == ATLAS_RUST_OK && value == 6,
            "self-merge did not double the count");
    require(atlas_rust_histogram_quantile(histogram, 5000, &bounds) == ATLAS_RUST_OK &&
                bounds.lower == 7 && bounds.upper == 7,
            "self-merge changed the observation");
    require(atlas_rust_histogram_quantile(histogram, 10000, &bounds) == ATLAS_RUST_OK &&
                bounds.lower == 13 && bounds.upper == 13,
            "merge lost the source maximum");

    require(atlas_rust_histogram_record_n(full, 9, UINT64_MAX) == ATLAS_RUST_OK,
            "maximum weight failed");
    require(atlas_rust_histogram_record(full, 1) == ATLAS_RUST_OVERFLOW,
            "record did not report overflow");
    require(atlas_rust_histogram_merge(full, full) == ATLAS_RUST_OVERFLOW,
            "self-merge did not report overflow");
    require(atlas_rust_histogram_count(full, &value) == ATLAS_RUST_OK && value == UINT64_MAX,
            "overflow changed the count");
    require(atlas_rust_histogram_min(full, &value) == ATLAS_RUST_OK && value == 9,
            "overflow changed the minimum");
    require(atlas_rust_histogram_max(full, &value) == ATLAS_RUST_OK && value == 9,
            "overflow changed the maximum");

    atlas_rust_histogram_destroy(NULL);
    atlas_rust_histogram_destroy(full);
    atlas_rust_histogram_destroy(source);
    atlas_rust_histogram_destroy(histogram);
}

static int compare_with_c(const atlas_histogram *c_histogram,
                          const AtlasRustHistogram *rust_histogram) {
    static const uint16_t percentiles[] = {0, 5000, 9500, 9900, 10000};
    size_t i;
    uint64_t count;

    if (atlas_rust_histogram_count(rust_histogram, &count) != ATLAS_RUST_OK ||
        count != c_histogram->count) {
        return 0;
    }

    if (count == 0) {
        uint64_t value = 17;
        if (atlas_rust_histogram_min(rust_histogram, &value) != ATLAS_RUST_EMPTY || value != 17 ||
            atlas_rust_histogram_max(rust_histogram, &value) != ATLAS_RUST_EMPTY || value != 17) {
            return 0;
        }
    } else {
        uint64_t min, max;
        if (atlas_rust_histogram_min(rust_histogram, &min) != ATLAS_RUST_OK ||
            atlas_rust_histogram_max(rust_histogram, &max) != ATLAS_RUST_OK ||
            min != c_histogram->min || max != c_histogram->max) {
            return 0;
        }
    }

    for (i = 0; i < sizeof percentiles / sizeof percentiles[0]; ++i) {
        atlas_bounds c_bounds = {0, 0};
        atlas_rust_bounds rust_bounds = {0, 0};
        int c_status = atlas_quantile(c_histogram, percentiles[i], &c_bounds);
        int rust_status = atlas_rust_histogram_quantile(rust_histogram, percentiles[i],
                                                       &rust_bounds);

        if (c_status != rust_status) return 0;
        if (c_status == ATLAS_OK &&
            (c_bounds.lower != rust_bounds.lower || c_bounds.upper != rust_bounds.upper)) {
            return 0;
        }
    }

    return 1;
}

static int print_report(const atlas_histogram *c_histogram,
                        const AtlasRustHistogram *rust_histogram) {
    static const uint16_t percentiles[] = {5000, 9500, 9900};
    static const char *names[] = {"p50", "p95", "p99"};
    atlas_rust_bounds bounds;
    uint64_t count, value;
    size_t i;

    if (atlas_rust_histogram_count(rust_histogram, &count) != ATLAS_RUST_OK) return 0;
    printf("{\"count\":%" PRIu64 ",\"min\":", count);
    if (count == 0) {
        fputs("null,\"max\":null,\"quantiles\":{", stdout);
    } else {
        if (atlas_rust_histogram_min(rust_histogram, &value) != ATLAS_RUST_OK) return 0;
        printf("%" PRIu64 ",\"max\":", value);
        if (atlas_rust_histogram_max(rust_histogram, &value) != ATLAS_RUST_OK) return 0;
        printf("%" PRIu64 ",\"quantiles\":{", value);
    }

    for (i = 0; i < sizeof percentiles / sizeof percentiles[0]; ++i) {
        int status = atlas_rust_histogram_quantile(rust_histogram, percentiles[i], &bounds);
        printf("%s\"%s\":", i == 0 ? "" : ",", names[i]);
        if (status == ATLAS_RUST_EMPTY) {
            fputs("null", stdout);
        } else if (status == ATLAS_RUST_OK) {
            printf("[%" PRIu64 ",%" PRIu64 "]", bounds.lower, bounds.upper);
        } else {
            return 0;
        }
    }
    fputs("}}\n", stdout);
    return fflush(stdout) == 0 && c_histogram->count == count;
}

int main(void) {
    atlas_histogram c_histogram;
    AtlasRustHistogram *rust_histogram;
    uint64_t value;
    int result;

    check_api_edges();
    atlas_init(&c_histogram);
    rust_histogram = atlas_rust_histogram_create();
    if (rust_histogram == NULL) {
        fputs("Rust C API: create returned NULL\n", stderr);
        return EXIT_FAILURE;
    }

    while ((result = scanf("%" SCNu64, &value)) == 1) {
        int rust_status = atlas_rust_histogram_record(rust_histogram, value);
        int c_status = atlas_record(&c_histogram, value, 1);

        if (rust_status != ATLAS_RUST_OK || c_status != ATLAS_OK) {
            fputs("Rust C API: recording input failed\n", stderr);
            atlas_rust_histogram_destroy(rust_histogram);
            return EXIT_FAILURE;
        }
    }

    if (result != EOF || ferror(stdin) || !compare_with_c(&c_histogram, rust_histogram) ||
        !print_report(&c_histogram, rust_histogram)) {
        fputs("Rust C API: input or conformance check failed\n", stderr);
        atlas_rust_histogram_destroy(rust_histogram);
        return EXIT_FAILURE;
    }

    atlas_rust_histogram_destroy(rust_histogram);
    return EXIT_SUCCESS;
}
