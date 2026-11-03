#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

usage() {
  echo "usage: $0 [--root DIR] [--mutations] PLAN" >&2
  exit 2
}

here=$(cd "$(dirname "$0")" && pwd)
root=""
mode=names
plan=""
while [ $# -gt 0 ]; do
  case "$1" in
    --root) [ $# -ge 2 ] || usage; root="$2"; shift 2 ;;
    --mutations) mode=mutations; shift ;;
    -*) usage ;;
    *) [ -z "$plan" ] || usage; plan="$1"; shift ;;
  esac
done
[ -n "$plan" ] || usage
[ -f "$plan" ] || { echo "no such plan: $plan" >&2; exit 2; }
plan=$(cd "$(dirname "$plan")" && pwd)/$(basename "$plan")
labels=$(cd "$(dirname "$0")" && pwd)/plan-labels.tsv
if [ -z "$root" ]; then
  root=$(git rev-parse --show-toplevel)
fi
cd "$root"

ctest_names() {
  local dir=$1
  [ -d "$dir" ] || return 0
  ctest --test-dir "$dir" -N 2>/dev/null | sed -nE 's/^ *Test +#[0-9]+: +//p'
}
cunit_names=$(ctest_names build/core-gcc-debug-san)

PLAN_LABELS="$here/plan-labels.tsv" PLAN_LABELS="$labels" CUNIT_NAMES="$cunit_names" exec python3 -B - "$plan" "$mode" <<'PY'
import fnmatch
import glob
import os
import re
import sys

plan_path, mode = sys.argv[1], sys.argv[2]
text = open(plan_path, encoding="utf-8").read()
lines = text.split("\n")

LABELS = {}
for row in open(os.environ["PLAN_LABELS"], encoding="utf-8"):
    ja, en = row.rstrip("\n").split("\t")
    LABELS[en] = (ja, en)


def column(header, en):
    return next(header.index(l) for l in LABELS[en] if l in header)


def stated_count(after):
    for label in LABELS["(N cases)"]:
        opening, inner, closing = label[0], label[1:-1], label[-1]
        pm = re.match(re.escape(opening) + "([^" + re.escape(closing) + "]*)" + re.escape(closing), after)
        if not pm:
            continue
        nm = re.search(re.escape(inner).replace("N", "([0-9]+)"), pm.group(1))
        if nm:
            return int(nm.group(1))
    return None

LABELS = {}
for row in open(os.environ["PLAN_LABELS"], encoding="utf-8").read().split("\n"):
    if row:
        ja, en = row.split("\t")
        LABELS[en] = (ja, en)


def label_re(en):
    return "(?:%s)" % "|".join(re.escape(l) for l in LABELS[en])


def column(header, en):
    for name in LABELS[en]:
        if name in header:
            return header.index(name)
    raise SystemExit(f"error: no column {en!r}")


def count_res():
    out = []
    for form in LABELS["(N cases)"]:
        before, after = form.split("N", 1)
        close, unit = after[-1], after[:-1]
        out.append((re.compile(r"\s*" + re.escape(before) + "([^%s]*)" % re.escape(close) + re.escape(close)),
                    re.compile(r"([0-9]+)" + re.escape(unit))))
    return out


def section(start_re, end_re):
    out, inside = [], False
    for line in lines:
        if not inside and re.match(start_re, line):
            inside = True
            continue
        if inside and re.match(end_re, line):
            break
        if inside:
            out.append(line)
    return out


def table_rows(block):
    rows = []
    for line in block:
        if not line.startswith("|") or re.match(r"^\|\s*-", line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append(cells)
    return rows


coverage = table_rows(section(r"^### 5\.3", r"^## 6\."))
header, body = coverage[0], coverage[1:]

if mode == "mutations":
    col = column(header, "How to break it")
    rows = sum(1 for r in body if len(r) > col and r[col] not in ("", "—", "-"))
    records = sum(1 for l in section(r"^## 9\.", r"^## 10\.")
                  if re.match(r"^\s*" + label_re("- Mutation check: "), l))
    print(f"mutations: {rows} rows, {records} recorded")
    sys.exit(0 if rows == records else 1)

CATEGORIES = ["golden", "fx", "py", "c", "sh", "cgolden", "cunit", "prop",
              "cli", "sample", "rtest"]
col = column(header, "Test or goal")

retired = set()
if os.path.exists("scripts/retired-tests.txt"):
    for l in open("scripts/retired-tests.txt", encoding="utf-8"):
        l = l.strip()
        if l and not l.startswith("#"):
            retired.add(l.split()[0])

cunit_names = os.environ.get("CUNIT_NAMES", "").split("\n")


def py_defs(module, func_glob):
    hits = []
    for path in glob.glob(f"tools/**/{module}.py", recursive=True):
        for m in re.finditer(r"^\s*def (\w+)\(", open(path, encoding="utf-8").read(), re.M):
            if fnmatch.fnmatchcase(m.group(1), func_glob):
                hits.append(f"{path}:{m.group(1)}")
    return hits


def golden_like(base, rest):
    files = set()
    for p in glob.glob(f"{base}/{rest}.ozen"):
        if os.path.exists(p[:-5] + ".out"):
            files.add(p[:-5])
    for p in glob.glob(f"{base}/{rest}"):
        if os.path.isfile(os.path.join(p, "expected.out")):
            files.add(p)
    return sorted(files)


def resolve(cat, rest):
    if cat == "cgolden":
        return golden_like("tests/golden", rest)
    if cat == "fx":
        hits = set(p[:-5] for p in glob.glob(f"spec/fixtures/{rest}.yaml"))
        hits |= set(p for p in glob.glob(f"spec/fixtures/{rest}") if os.path.isdir(p))
        return sorted(hits)
    if cat == "py":
        if "." not in rest:
            return []
        module, func = rest.split(".", 1)
        return py_defs(module, func)
    if cat == "cunit":
        return [n for n in cunit_names if n and fnmatch.fnmatchcase(n, rest)]
    if cat == "sh":
        return sorted(glob.glob(f"scripts/{rest}"))
    if cat == "prop":
        return sorted(glob.glob(f"tests/props/{rest}.py"))
    if cat == "cli":
        return sorted(glob.glob(f"tests/cli/{rest}.sh"))
    if cat == "sample":
        return sorted(glob.glob(f"samples/{rest}/expect.yaml"))
    if cat == "rtest":
        return sorted(glob.glob(f"renderers/test/{rest}.test.mjs"))
    return []


name_re = re.compile(r"`((%s)/([^`\s]+))`" % "|".join(CATEGORIES))
wanted = {}
errors = 0
for r in body:
    if len(r) <= col:
        continue
    cell = r[col]
    for m in name_re.finditer(cell):
        name, cat, rest = m.group(1), m.group(2), m.group(3)
        count = None
        if "*" in rest:
            after = cell[m.end():]
            nm = None
            for paren_re, num_re in count_res():
                pm = paren_re.match(after)
                nm = num_re.search(pm.group(1)) if pm else None
                if nm:
                    break
            if not nm:
                print(f"error: {name} has * but no count", file=sys.stderr)
                errors += 1
                continue
            count = int(nm.group(1))
        prev = wanted.get(name)
        if prev is not None and prev[2] != count:
            print(f"error: {name} has conflicting counts {prev[2]} and {count}", file=sys.stderr)
            errors += 1
        wanted[name] = (cat, rest, count)

found = {c: set() for c in CATEGORIES}
missing = 0
total = 0
for name in sorted(wanted):
    cat, rest, count = wanted[name]
    if name in retired:
        continue
    total += 1
    hits = resolve(cat, rest)
    if count is None:
        ok = len(hits) >= 1
    else:
        ok = len(hits) == count
    found[cat].update(hits)
    if not ok:
        missing += 1
        want = "1" if count is None else str(count)
        print(f"missing: {name} (found {len(hits)}, want {want})", file=sys.stderr)

print(" ".join(f"{c}={len(found[c])}" for c in CATEGORIES))
print(f"plan-tests: {total} names, {missing} missing")
sys.exit(0 if missing == 0 and errors == 0 else 1)
PY
