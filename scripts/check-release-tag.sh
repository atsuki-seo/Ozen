#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
usage() {
  echo "usage: $0 TAG | --workflow-first-step FILE" >&2
  exit 2
}

if [ "${1:-}" = --workflow-first-step ]; then
  [ $# -eq 2 ] || usage
  exec python3 -B - "$2" <<'PY'
import sys
import yaml

doc = yaml.safe_load(open(sys.argv[1], encoding="utf-8")) or {}
bad = 0
for name, job in (doc.get("jobs") or {}).items():
    steps = job.get("steps") or []
    at = next((i for i, s in enumerate(steps) if str(s.get("uses", "")).startswith("actions/checkout")), None)
    first = steps[at + 1] if at is not None and at + 1 < len(steps) else {}
    if 'scripts/check-release-tag.sh "$GITHUB_REF_NAME"' in str(first.get("run", "")):
        print(f"ok {name}")
    else:
        print(f"bad {name}")
        bad += 1
sys.exit(1 if bad or not doc.get("jobs") else 0)
PY
fi

[ $# -eq 1 ] || usage
want="v$(cat "$root/VERSION")"
if [ "$1" = "$want" ]; then
  echo "release tag: $1"
  exit 0
fi
echo "release tag: $1 does not match VERSION (want $want)" >&2
exit 1
