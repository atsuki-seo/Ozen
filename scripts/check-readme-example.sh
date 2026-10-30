#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
readme="$root/README.md"
bin="$root/build/gcc-release/ozenproto"
ozen="$root/build/core-gcc-release/ozen"
while [ $# -gt 0 ]; do
  case "$1" in
    --readme) readme="$2"; shift 2 ;;
    --bin) bin="$2"; shift 2 ;;
    --ozen) ozen="$2"; shift 2 ;;
    *) echo "usage: $0 [--readme FILE] [--bin OZENPROTO] [--ozen OZEN]" >&2; exit 2 ;;
  esac
done

exec python3 -B - "$readme" "$bin" "$ozen" <<'PY'
import os, re, shlex, subprocess, sys, tempfile
readme, proto, ozen = sys.argv[1:4]
lines = open(readme, encoding="utf-8").read().split("\n")
examples, files, name, run, fresh = [], [], None, None, False
i = 0
while i < len(lines):
    line = lines[i]
    m = re.fullmatch(r"<!-- (file|run): (.+) -->", line)
    if m and m.group(1) == "file":
        name = m.group(2)
    elif m:
        run = m.group(2)
    elif line in ("```ozen", "```text"):
        j = i + 1
        while j < len(lines) and lines[j] != "```":
            j += 1
        body = "\n".join(lines[i + 1:j]) + "\n"
        if line == "```ozen":
            files = [] if fresh else files
            files = [f for f in files if f[0] != name] + [(name or f"example{len(examples)}.ozen", body)]
            fresh = False
        elif files:
            examples.append((list(files), run, body))
            run, fresh = None, True
        name = None
        i = j
    i += 1
mismatches = 0
for k, (group, run, want) in enumerate(examples):
    with tempfile.TemporaryDirectory() as tmp:
        for fname, body in group:
            path = os.path.join(tmp, fname)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(body)
        words = shlex.split(run) if run else ["ozenproto", group[-1][0]]
        words[0] = {"ozenproto": proto, "ozen": ozen}.get(words[0], words[0])
        got = subprocess.run(words, cwd=tmp, capture_output=True, text=True).stdout
        if got != want:
            mismatches += 1
            print(f"example {k} differs:\n{got}--- README ---\n{want}", file=sys.stderr)
print(f"readme: {len(examples)} examples, {mismatches} mismatches")
sys.exit(0 if examples and mismatches == 0 else 1)
PY
