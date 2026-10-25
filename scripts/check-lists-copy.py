#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import re
import sys

import yaml


def plan_identifiers(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    ids, inside, seen_header = set(), False, False
    for line in lines:
        if re.match(r"^### 4\.7 ", line):
            inside = True
            continue
        if inside and re.match(r"^#{2,4} ", line):
            break
        if not inside or not line.startswith("|") or re.match(r"^\|\s*-", line):
            continue
        if not seen_header:
            seen_header = True
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cells[0] == "合計":
            continue
        ids.update(re.findall(r"`([^`]+)`", "|".join(cells[2:])))
    return ids


def yaml_identifiers(path):
    with open(path, encoding="utf-8") as f:
        doc = yaml.safe_load(f)
    ids = set()
    for e in (doc or {}).get("entries", []):
        ids.add(str(e["id"]))
        if e.get("owner") is not None:
            ids.add(str(e["owner"]))
    return ids


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} PLAN CONCEPT_LISTS_YAML", file=sys.stderr)
        return 2
    want = plan_identifiers(argv[1])
    got = yaml_identifiers(argv[2])
    for name in sorted(want - got):
        print(f"missing in yaml: {name}", file=sys.stderr)
    for name in sorted(got - want):
        print(f"not in plan: {name}", file=sys.stderr)
    diff = len(want ^ got)
    print(f"copy: {len(want)} identifiers, {diff} differences")
    return 0 if diff == 0 and want else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
