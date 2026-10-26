#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
ozen=${OZEN:-$root/build/core-gcc-release/ozen}
tools=$root/tools/stdcheck
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

mkdir -p "$work/empty"
(cd "$work/empty" && "$ozen" debug definitions) > "$work/defs.json"
cp "$root/spec/sources/search-log.yaml" "$work/log.yaml"

mutate() {
  python3 - "$work/defs.json" "$work/m.json" "$1" <<'PY'
import json, sys
src, dst, what = sys.argv[1:4]
d = json.load(open(src, encoding="utf-8"))
defs = d["definitions"]
def find(kind, name):
    return next(x for x in defs if x["kind"] == kind and x["name"] == name)
def field(members, name):
    for m in members:
        if m.get("field") == name:
            return m
        if "section" in m:
            r = field(m["members"], name)
            if r:
                return r
if what == "upper-action":
    find("action", "wash")["name"] = "Wash"
elif what == "provisional-point":
    field(find("ingredient", "Rice")["members"], "density")["values"][0]["text"] = "0.83 g/ml"
elif what == "fct-inline":
    chicken = find("ingredient", "Chicken")
    parts = next(m for m in chicken["members"] if m.get("section") == "parts")
    field(parts["members"], "refuse")["values"][0]["text"] = "10..20 %"
elif what == "unprovided":
    defs.remove(find("kind", "Blender"))
elif what == "drop-vessel":
    defs.remove(find("vessel", "Platter"))
elif what == "no-source":
    field(find("ingredient", "Rice")["members"], "density")["annotations"] = []
elif what == "quote":
    rice = find("action", "cookRice")
    cooker = next(m for m in rice["members"] if m.get("variant") == "cooker")
    next(m for m in cooker["members"] if "uses" in m)["for"]["text"] = "42..55 min"
json.dump(d, open(dst, "w", encoding="utf-8"))
PY
}

expect() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  got=$("$@" 2>/dev/null | tail -n 1) || true
  if [[ ! $got =~ $want ]]; then
    echo "FAIL $name: got '$got', want /$want/" >&2
    failed=$((failed + 1))
  fi
}

L=(python3 -B "$tools/lists.py" "$root/spec/coverage/concept-lists.yaml" "$root/spec/coverage/concept-relations.yaml")
expect lists-ok ' 0 missing; values: [0-9]+ checked, 0 mismatched; relations: [0-9]+ listed, 0 mismatched$' "${L[@]}" "$work/defs.json"
mutate drop-vessel
expect lists-missing ' 1 missing;' "${L[@]}" "$work/m.json"
expect static-ok ' 0 naming, 0 provisional-point, 0 fct-inline, 0 capability-unprovided$' python3 -B "$tools/static.py" "$work/defs.json"
mutate upper-action
expect static-naming ' 1 naming,' python3 -B "$tools/static.py" "$work/m.json"
mutate provisional-point
expect static-point ' 1 provisional-point,' python3 -B "$tools/static.py" "$work/m.json"
mutate fct-inline
expect static-fct ' 1 fct-inline,' python3 -B "$tools/static.py" "$work/m.json"
mutate unprovided
expect static-unprovided ' 1 capability-unprovided$' python3 -B "$tools/static.py" "$work/m.json"
expect provenance-ok ' 0 without source$' python3 -B "$tools/provenance.py" "$work/defs.json"
mutate no-source
expect provenance-missing ' 1 without source$' python3 -B "$tools/provenance.py" "$work/m.json"
S=(python3 -B "$tools/sources.py")
expect sources-ok ' 0 missing fields, 0 quote mismatches, 0 provisional without search, 0 pdf in repo$' "${S[@]}" "$work/defs.json" "$root/stdlib/sources.ozen" "$work/log.yaml"
mutate quote
expect sources-quote ' 1 quote mismatches,' "${S[@]}" "$work/m.json" "$root/stdlib/sources.ozen" "$work/log.yaml"
sed -i '/ingredients\.Rice\.props\.density/d' "$work/log.yaml"
expect sources-unsearched ' 1 provisional without search,' "${S[@]}" "$work/defs.json" "$root/stdlib/sources.ozen" "$work/log.yaml"

echo "test-stdcheck: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
