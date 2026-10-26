"""Reproducible local timing, not a cross-machine performance claim."""
import argparse
import json
import platform
from pathlib import Path
import random
import statistics
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--binary", required=True)
parser.add_argument("--samples", type=int, default=100000)
parser.add_argument("--runs", type=int, default=5)
args = parser.parse_args()
if args.samples < 1 or args.runs < 1:
    parser.error("samples and runs must be positive")
rng = random.Random(42)
data = "\n".join(str(rng.randrange(1, 1000000)) for _ in range(args.samples)).encode()
elapsed = []
for _ in range(args.runs):
    start = time.perf_counter()
    result = subprocess.run([str(Path(args.binary).resolve()), "--json"], input=data, capture_output=True, check=True)
    elapsed.append(time.perf_counter() - start)
    assert json.loads(result.stdout)["count"] == args.samples
print(json.dumps({"platform": platform.platform(), "samples": args.samples, "runs": args.runs,
                  "median_seconds": statistics.median(elapsed), "seconds": elapsed}, indent=2))
