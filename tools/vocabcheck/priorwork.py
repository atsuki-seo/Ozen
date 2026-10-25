#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import re

import yaml

BASIS = re.compile(r"concept §[0-9]|\bS[0-9]+\b|\bD[0-9]+\b")


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} ELEMENTS_YAML MAPPING_MD", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as f:
        doc = yaml.safe_load(f) or {}
    sources = doc.get("sources") or {}
    elements = doc.get("elements") or []
    rows = {}
    with open(argv[2], encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^\|\s*`([^`]+)`\s*\|(.*)\|(.*)\|\s*$", line)
            if m:
                rows[m.group(1)] = (m.group(2).strip(), m.group(3).strip())
    unmapped = without_page = 0
    used_sources = set()
    for e in elements:
        eid = e.get("id")
        if e.get("source") in sources:
            used_sources.add(e.get("source"))
        if not isinstance(e.get("page"), int) or e["page"] < 1:
            without_page += 1
            print(f"without page: {eid}", file=sys.stderr)
        ozen, basis = rows.get(eid, ("", ""))
        if ozen.startswith("No counterpart"):
            ok = len(ozen.split(":", 1)[-1].strip()) > 0 and ":" in ozen
        else:
            ok = bool(ozen) and bool(BASIS.search(basis))
        if not ok:
            unmapped += 1
            print(f"unmapped: {eid}", file=sys.stderr)
    print(f"prior-work: {len(elements)} elements from {len(used_sources)} sources, "
          f"{unmapped} unmapped, {without_page} without page")
    return 0 if len(used_sources) == len(sources) and sources and not unmapped and not without_page else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
