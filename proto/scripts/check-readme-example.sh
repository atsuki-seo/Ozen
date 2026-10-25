#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
readme="$root/README.md"
bin="$root/build/gcc-release/ozenproto"
while [ $# -gt 0 ]; do
  case "$1" in
    --readme) readme="$2"; shift 2 ;;
    --bin) bin="$2"; shift 2 ;;
    *) echo "usage: $0 [--readme FILE] [--bin OZENPROTO]" >&2; exit 2 ;;
  esac
done

exec python3 -B - "$readme" "$bin" <<'PY'
import os, re, subprocess, sys, tempfile
readme, binary = sys.argv[1:3]
text = open(readme, encoding="utf-8").read()
pairs = re.findall(r"```ozen\n(.*?)```\s*\n(?:[^`]*?\n)?```text\n(.*?)```", text, re.S)
mismatches = 0
with tempfile.TemporaryDirectory() as tmp:
    for i, (src, want) in enumerate(pairs):
        path = os.path.join(tmp, f"example{i}.ozen")
        with open(path, "w", encoding="utf-8") as f:
            f.write(src)
        got = subprocess.run([binary, path], capture_output=True, text=True).stdout
        if got != want:
            mismatches += 1
            print(f"example {i} differs:\n{got}--- README ---\n{want}", file=sys.stderr)
print(f"readme: {len(pairs)} examples, {mismatches} mismatches")
sys.exit(0 if pairs and mismatches == 0 else 1)
PY
