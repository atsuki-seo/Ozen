# SPDX-License-Identifier: MIT OR Apache-2.0
import os
import re
import subprocess
import sys
import unicodedata
from fractions import Fraction

import yaml

from common import Defs, fmt, quantity, source_of, walk_values

REQUIRED = ("url", "title", "retrieved", "location", "quote")
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def quote_numbers(text):
    text = unicodedata.normalize("NFKC", str(text))
    return {Fraction(n) for n in re.findall(r"[0-9]+(?:\.[0-9]+)?", text)}


def sources(defs):
    out = {}
    for s in defs.of_kind("source"):
        entry = {}
        for m in s["members"]:
            if "field" in m and m["values"]:
                text = m["values"][0]["text"]
                entry[m["field"]] = text[1:-1] if text.startswith('"') else text
        out[s["name"]] = entry
    return out


def main(argv):
    if len(argv) != 4:
        print(f"usage: {argv[0]} DEFINITIONS SOURCES_OZEN SEARCH_LOG", file=sys.stderr)
        return 2
    defs = Defs(argv[1])
    table = sources(defs)
    covered = set()
    if os.path.exists(argv[3]):
        with open(argv[3], encoding="utf-8") as f:
            for entry in (yaml.safe_load(f) or {}).get("searches") or []:
                if all(entry.get(k) for k in ("date", "queries", "consulted", "reason")):
                    covered.update(entry.get("covers") or [])
    primary = provisional = missing = mismatches = unsearched = 0
    checked = set()
    for d in defs.all:
        if not d["module"].startswith("std."):
            continue
        for path, value in walk_values(d):
            src = source_of(value)
            if src is None:
                continue
            kind = (table.get(src) or {}).get("kind")
            key = fmt(path)
            if kind == "provisional":
                provisional += 1
                if key not in covered:
                    unsearched += 1
                    print(f"provisional without search: {key}", file=sys.stderr)
            elif kind == "primary":
                primary += 1
                entry = table.get(src) or {}
                if src not in checked:
                    checked.add(src)
                    for field in REQUIRED:
                        if not entry.get(field):
                            missing += 1
                            print(f"missing field: {src}.{field}", file=sys.stderr)
                q = quantity(value["text"])
                nums = quote_numbers(entry.get("quote", ""))
                if q is None or q[0] not in nums or q[1] not in nums:
                    mismatches += 1
                    print(f"quote mismatch: {key} = {value['text']!r}", file=sys.stderr)
    pdfs = subprocess.run(["git", "ls-files", "*.pdf"], capture_output=True, text=True, cwd=ROOT).stdout.split()
    print(f"sources: {primary} primary, {provisional} provisional, {missing} missing fields, "
          f"{mismatches} quote mismatches, {unsearched} provisional without search, {len(pdfs)} pdf in repo")
    return 1 if missing or mismatches or unsearched or pdfs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
