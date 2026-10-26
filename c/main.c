#include "histogram.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    atlas_histogram h;
    atlas_bounds b;
    char line[66], *start, *end;
    uint64_t number = 0, value;
    unsigned i;
    int ch;
    size_t length;
    const unsigned percentiles[] = {5000, 9500, 9900};
    const char *names[] = {"p50", "p95", "p99"};
    if (argc != 2 || strcmp(argv[1], "--json") != 0) {
        fputs("usage: latency-atlas-c --json < samples.txt\n", stderr); return 2;
    }
    atlas_init(&h);
    for (;;) {
        length = 0;
        while ((ch = getchar()) != EOF) {
            if (ch == 0 || length == 64) {
                fprintf(stderr, "line %" PRIu64 ": NUL byte or line exceeds 64 bytes\n", number + 1);
                return 2;
            }
            line[length++] = (char)ch;
            if (ch == '\n') break;
        }
        if (length == 0 && ch == EOF) break;
        line[length] = '\0';
        ++number;
        start = line;
        while (isspace((unsigned char)*start)) ++start;
        end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1])) --end;
        *end = '\0';
        if (*start == '\0') continue;
        for (end = start; *end; ++end) {
            if (*end < '0' || *end > '9') {
                fprintf(stderr, "line %" PRIu64 ": expected unsigned integer nanoseconds\n", number); return 2;
            }
        }
        errno = 0;
        value = strtoull(start, &end, 10);
        if (errno == ERANGE || *end != '\0') {
            fprintf(stderr, "line %" PRIu64 ": outside u64 range\n", number); return 2;
        }
        if (atlas_record(&h, value, 1) != ATLAS_OK) { fputs("count overflow\n", stderr); return 2; }
    }
    if (ferror(stdin)) { fputs("input read failed\n", stderr); return 2; }
    printf("{\"count\":%" PRIu64 ",\"min\":", h.count);
    if (h.count) printf("%" PRIu64, h.min); else fputs("null", stdout);
    fputs(",\"max\":", stdout);
    if (h.count) printf("%" PRIu64, h.max); else fputs("null", stdout);
    fputs(",\"quantiles\":{", stdout);
    for (i = 0; i < 3; ++i) {
        printf("%s\"%s\":", i ? "," : "", names[i]);
        if (atlas_quantile(&h, percentiles[i], &b) == ATLAS_EMPTY) fputs("null", stdout);
        else printf("[%" PRIu64 ",%" PRIu64 "]", b.lower, b.upper);
    }
    fputs("}}\n", stdout);
    if (fflush(stdout) != 0) return 2;
    return 0;
}
