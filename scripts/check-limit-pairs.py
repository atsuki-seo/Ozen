# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import subprocess
import sys

LIMITS = {
    "--max-steps": "MAC001",
    "--max-nodes": "MAC003",
    "--max-depth": "SYN011",
    "--max-input-bytes": "SYN010",
}


def exact_entries(root):
    for d, dirs, files in os.walk(root):
        dirs.sort()
        if os.path.basename(d).endswith("-exact") and "expected.out" in files:
            yield d, d, []
            dirs[:] = []
            continue
        for f in sorted(files):
            if f.endswith("-exact.ozen"):
                yield os.path.join(d, f), d, [f]


def args_of(entry, rundir, target):
    if target:
        first = open(entry, encoding="utf-8", errors="replace").readline()
        return first[len("// args: "):].split() if first.startswith("// args: ") else ["check"]
    path = os.path.join(rundir, "args")
    return open(path, encoding="utf-8").read().split() if os.path.exists(path) else ["check"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ap.add_argument("golden")
    a = ap.parse_args()
    ozen = os.path.abspath(a.ozen)
    pairs = bad = 0
    for entry, rundir, target in exact_entries(a.golden):
        args = args_of(entry, rundir, target)
        flags = [k for k, v in enumerate(args) if v in LIMITS and k + 1 < len(args) and args[k + 1].isdigit()]
        if not flags:
            continue
        pairs += 1
        k = flags[0]
        lowered = args[:k + 1] + [str(int(args[k + 1]) - 1)] + args[k + 2:]
        out = subprocess.run([ozen, *lowered, *target], cwd=rundir, capture_output=True, text=True,
                             timeout=30).stdout
        if f" {LIMITS[args[k]]} " not in out:
            bad += 1
            print(f"not exact: {entry}: {args[k]} {int(args[k + 1]) - 1} gives no {LIMITS[args[k]]}", file=sys.stderr)
    print(f"limit-pairs: {pairs} pairs, {bad} not exact")
    return 0 if pairs and bad == 0 else 1


sys.exit(main())
