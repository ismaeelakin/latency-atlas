# Reproducible local measurements

Build release binaries, then run:

```sh
python tests/benchmark.py --binary target/release/latency-atlas --samples 100000 --runs 5
python tests/benchmark.py --binary build/latency-atlas-c --samples 100000 --runs 5
```

The script generates a deterministic synthetic distribution outside the timer, launches a new process for each run, writes samples through a pipe, validates the resulting count, and reports individual durations, median, platform, and input size. It measures process startup, input parsing, pipe transfer, aggregation, and output together. It is not a microbenchmark of bucket updates or a claim about production workloads.

Compare implementations only with the same compiler mode, machine, input, run count, and power settings. Windows antivirus, filesystem caches, and background tasks can dominate short runs. Report compiler versions and raw durations alongside medians. The constant storage bound follows from the data structure; this script does not measure peak resident memory.
