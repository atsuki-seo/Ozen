#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import glob
import os
import re
import subprocess
import unicodedata
from fractions import Fraction

import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import qty as qtymod

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REQUIRED = ("url", "title", "retrieved", "location", "quote")


def fmt_path(parts):
    out = ""
    for p in parts:
        out += f"[{p}]" if isinstance(p, int) else ("." if out else "") + str(p)
    return out


def attributes(vocab_dir):
    found = []

    def walk(x, path):
        if isinstance(x, dict):
            if "source" in x and isinstance(x.get("value"), str):
                found.append((fmt_path(path), x["value"], x["source"]))
                return
            for k, v in x.items():
                walk(v, path + [k])
        elif isinstance(x, list):
            for i, v in enumerate(x):
                walk(v, path + [i])
    for path in sorted(glob.glob(os.path.join(vocab_dir, "*.yaml"))):
        if os.path.basename(path) == "sources.yaml":
            continue
        with open(path, encoding="utf-8") as f:
            walk(yaml.safe_load(f), [])
    return found


def quote_numbers(text):
    text = unicodedata.normalize("NFKC", str(text))
    return {Fraction(n) for n in re.findall(r"[0-9]+(?:\.[0-9]+)?", text)}


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} VOCAB_DIR SEARCH_LOG", file=sys.stderr)
        return 2
    vocab_dir, log_path = argv[1], argv[2]
    with open(os.path.join(vocab_dir, "sources.yaml"), encoding="utf-8") as f:
        sources = (yaml.safe_load(f) or {}).get("sources") or {}
    with open(os.path.join(vocab_dir, "units.yaml"), encoding="utf-8") as f:
        units = set(((yaml.safe_load(f) or {}).get("units") or {}))
    covered = set()
    if os.path.exists(log_path):
        with open(log_path, encoding="utf-8") as f:
            for entry in (yaml.safe_load(f) or {}).get("searches") or []:
                if all(entry.get(k) for k in ("date", "queries", "consulted", "reason")):
                    covered.update(entry.get("covers") or [])
    primary = provisional = missing = mismatches = unsearched = 0
    checked_sources = set()
    for path, value, src in attributes(vocab_dir):
        kind = (sources.get(src) or {}).get("kind")
        if kind == "provisional":
            provisional += 1
            if path not in covered:
                unsearched += 1
                print(f"provisional without search: {path}", file=sys.stderr)
        elif kind == "primary":
            primary += 1
            entry = sources.get(src) or {}
            if src not in checked_sources:
                checked_sources.add(src)
                for field in REQUIRED:
                    if not entry.get(field):
                        missing += 1
                        print(f"missing field: {src}.{field}", file=sys.stderr)
            q = qtymod.parse(value, units)
            nums = quote_numbers(entry.get("quote", ""))
            if not q or q.lo not in nums or q.hi not in nums:
                mismatches += 1
                print(f"quote mismatch: {path} = {value!r}, quote {entry.get('quote')!r}", file=sys.stderr)
    pdfs = subprocess.run(["git", "ls-files", "*.pdf"], capture_output=True, text=True, cwd=ROOT).stdout.split()
    print(f"sources: {primary} primary, {provisional} provisional, {missing} missing fields, "
          f"{mismatches} quote mismatches, {unsearched} provisional without search, {len(pdfs)} pdf in repo")
    return 1 if missing or mismatches or unsearched or pdfs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
