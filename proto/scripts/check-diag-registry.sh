#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
registry="$root/proto/src/diag_codes.def"
names="$root/proto/tests/diag-names.txt"
outputs="$root/proto/tests/golden"
categories="SYN NAM TYP MAC"
while [ $# -gt 0 ]; do
  case "$1" in
    --registry) registry="$2"; shift 2 ;;
    --names) names="$2"; shift 2 ;;
    --outputs) outputs="$2"; shift 2 ;;
    --categories) categories="$2"; shift 2 ;;
    *) echo "usage: $0 [--registry FILE] [--names FILE] [--outputs DIR] [--categories LIST]" >&2; exit 2 ;;
  esac
done

exec python3 -B - "$registry" "$names" "$outputs" "$categories" <<'PY'
import os
import re
import sys

registry, names_path, outputs, categories = sys.argv[1:5]
cats = set(categories.split())
entries = []
for line in open(registry, encoding="utf-8"):
    m = re.match(r"^\s*OZPI_DIAG\(\s*([^,\s]+)\s*,\s*([^,\s]+)\s*,\s*([^)\s]+)\s*\)", line)
    if m:
        entries.append(m.groups())
listed = [l.strip() for l in open(names_path, encoding="utf-8") if l.strip() and not l.startswith("#")] \
    if os.path.exists(names_path) else []
texts = []
for d, _, files in os.walk(outputs):
    for f in files:
        if f.endswith(".out"):
            texts.append(open(os.path.join(d, f), encoding="utf-8", errors="replace").read())
blob = "\n".join(texts)
codes = [e[0] for e in entries]
full = [f"{e[1]}:{e[2]}" for e in entries]
dup_codes = len(codes) - len(set(codes))
dup_names = len(full) - len(set(full))
bad = 0
for code, cat, _ in entries:
    if not re.fullmatch(r"[A-Z]{3}[0-9]{3}", code) or cat not in cats or not code.startswith(cat):
        bad += 1
        print(f"bad code: {code} {cat}", file=sys.stderr)
unlisted = sorted(set(full) ^ set(listed))
for n in unlisted:
    print(f"unlisted: {n}", file=sys.stderr)
unused = 0
for code, cat, name in entries:
    if not re.search(rf"\b{re.escape(code)} {re.escape(name)}\b", blob):
        unused += 1
        print(f"unused: {code} {cat}:{name}", file=sys.stderr)
print(f"diag: {len(entries)} codes, {dup_codes} duplicate codes, {dup_names} duplicate names, "
      f"{bad} bad codes, {len(unlisted)} unlisted, {unused} unused")
sys.exit(0 if not (dup_codes or dup_names or bad or unlisted or unused) and entries else 1)
PY
