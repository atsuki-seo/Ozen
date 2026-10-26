# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys

PHASES = "DEFGHIJKL"


def registry(path):
    out = {}
    for line in open(path, encoding="utf-8"):
        m = re.match(r"^\s*OZNI_DIAG\(\s*([A-Z]{3}[0-9]{3})\s*,\s*([A-Z]{3})\s*,\s*(\w+)\s*,\s*([A-Z]+)\s*\)", line)
        if m:
            out[f"{m.group(2)}:{m.group(3)}"] = m.group(4).lower()
    return out


def std_checks(stdlib):
    out = {}
    sev = {"warn": "warning", "error": "error", "info": "info"}
    for d, _, files in os.walk(stdlib):
        for f in files:
            if not f.endswith(".ozen"):
                continue
            for line in open(os.path.join(d, f), encoding="utf-8"):
                m = re.match(r"^\s*check\s+([A-Z]{3})([0-9]{3})\s+(\w+)\s*:.*?(?:\belse\s+(\w+))?\s*$", line)
                if m:
                    out[f"{m.group(1)}:{m.group(3)}"] = sev.get(m.group(4) or "error", m.group(4))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase", default=None)
    ap.add_argument("examples")
    ap.add_argument("registry")
    ap.add_argument("stdlib")
    a = ap.parse_args()
    known = registry(a.registry)
    known.update(std_checks(a.stdlib))
    limit = PHASES.index(a.phase) if a.phase else len(PHASES)
    counts = {"severity": [0, 0], "category": [0, 0]}
    for line in open(a.examples, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        kind, example, diag, severity, phase = line.rstrip("\n").split("\t")
        if PHASES.index(phase) > limit:
            continue
        counts[kind][0] += 1
        ok = diag in known and (kind == "category" or known[diag] == severity)
        if not ok:
            counts[kind][1] += 1
            got = known.get(diag, "not registered")
            print(f"mismatch: {example}: {diag} is {got}, want {severity if kind == 'severity' else 'registered'}",
                  file=sys.stderr)
    s, c = counts["severity"], counts["category"]
    print(f"severity: {s[0]} examples, {s[1]} mismatched; categories: {c[0]} examples, {c[1]} mismatched")
    return 0 if s[1] == 0 and c[1] == 0 else 1


sys.exit(main())
