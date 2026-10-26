# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import collections
import glob
import os
import re
import subprocess
import sys

HEADINGS = ["書き心地とバグ量", "環境ごとの結果", "ファズ", "ユーザーが決めること"]


def sections(text):
    out, name = {}, None
    for line in text.split("\n"):
        m = re.match(r"^## (?:\d+\. )?(.+?)\s*$", line)
        if m:
            name = m.group(1)
            out[name] = []
        elif name is not None:
            out[name].append(line)
    return out


def table(lines, first_header):
    rows, inside = [], False
    for line in lines:
        if not line.startswith("|"):
            if inside:
                break
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not inside:
            if cells[0] == first_header:
                inside = True
            continue
        if re.match(r"^-+$", cells[0]):
            continue
        rows.append(cells)
    return rows if inside else None


def unquote(cell):
    return cell.strip().strip("`")


def count_lines(root, patterns):
    total = 0
    files = set()
    for pat in patterns:
        hits = [p for p in glob.glob(os.path.join(root, pat), recursive=True) if os.path.isfile(p)]
        if not hits:
            return None
        files.update(hits)
    for path in files:
        with open(path, "rb") as f:
            total += f.read().count(b"\n")
    return total


def git(repo, *args):
    return subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("report")
    ap.add_argument("results")
    ap.add_argument("defects")
    ap.add_argument("--fuzz")
    ap.add_argument("--repo")
    ap.add_argument("--since", default="plan-start")
    a = ap.parse_args()
    fuzz_path = a.fuzz or os.path.join(os.path.dirname(os.path.abspath(a.results)), "fuzz", "summary.txt")
    repo = a.repo or git(".", "rev-parse", "--show-toplevel").stdout.strip()

    text = open(a.report, encoding="utf-8").read()
    secs = sections(text)
    checks = 0
    failed = 0

    def check(name, ok, detail=""):
        nonlocal checks, failed
        checks += 1
        if not ok:
            failed += 1
            print(f"FAIL {name}: {detail}", file=sys.stderr)

    order = [h for h in secs if h in HEADINGS]
    check("headings", order == HEADINGS, f"got {order}")

    body1 = secs.get(HEADINGS[0], [])
    lines_table = table(body1, "ファイルの組")
    if not lines_table:
        check("line-count table", False, "missing")
    for row in lines_table or []:
        if len(row) < 3:
            check(f"lines {row[0]}", False, "short row")
            continue
        patterns = [unquote(p) for p in re.findall(r"`[^`]+`", row[1])]
        got = count_lines(repo, patterns)
        want = row[2].replace(",", "")
        check(f"lines {row[0]}", got is not None and str(got) == want,
              f"table says {row[2]}, wc -l gives {got}")

    env_table = table(secs.get(HEADINGS[1], []), "環境") or []
    envs = {unquote(r[0]): r for r in env_table if len(r) >= 3}
    seen = set()
    for path in sorted(glob.glob(os.path.join(a.results, "*.txt"))):
        base = os.path.basename(path)[:-4]
        kind, _, env = base.partition("-")
        value = open(path, encoding="utf-8").read().strip()
        row = envs.get(env)
        seen.add(env)
        if kind == "golden":
            m = re.match(r"golden: (\d+) passed, (\d+) failed, (\d+) timed out$", value)
            want = f"{m.group(1)}/{sum(int(x) for x in m.groups())}" if m else value
            got = row[1] if row else None
        elif kind == "digest":
            want = value
            got = unquote(row[2]) if row else None
        else:
            check(f"result {base}", False, "unknown result kind")
            continue
        check(f"result {base}", got == want, f"table says {got}, {path} says {want}")
    extra = [e for e, r in envs.items() if e not in seen and (r[1] != "—" or r[2] != "—")]
    check("environment rows", not extra, f"rows without results: {extra}")

    fuzz = open(fuzz_path, encoding="utf-8").read().strip() if os.path.exists(fuzz_path) else None
    fuzz_body = "\n".join(secs.get(HEADINGS[2], []))
    check("fuzz", fuzz is not None and f"`{fuzz}`" in fuzz_body, f"{fuzz_path} says {fuzz}")

    rows = []
    with open(a.defects, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            if line.strip():
                rows.append(dict(zip(header, line.rstrip("\n").split("\t"))))
    by_method = collections.Counter(r["method"] for r in rows)
    want = dict(by_method)
    want["合計"] = len(rows)
    defect_table = table(body1, "検出した手段") or []
    got = {unquote(r[0]): int(r[1]) for r in defect_table if len(r) >= 2 and r[1].isdigit()}
    check("defect counts", got == want, f"table says {got}, {a.defects} says {want}")

    bad = [r["commit"] for r in rows
           if git(repo, "merge-base", "--is-ancestor", r["commit"], "HEAD").returncode != 0]
    check("defect commits", not bad, f"not ancestors of HEAD: {bad}")

    listed = set()
    for r in rows:
        full = git(repo, "rev-parse", "--verify", "-q", r["commit"] + "^{commit}").stdout.strip()
        listed.add(full)
    log = git(repo, "log", "--format=%H %s", f"{a.since}..HEAD", "--", "proto/src").stdout.split("\n")
    fixes = [l.split(" ", 1)[0] for l in log if l and re.match(r"^\S+ fix", l, re.I)]
    missing = [h[:7] for h in fixes if h not in listed]
    check("fix commits", not missing, f"fix commits missing from {a.defects}: {missing}")

    print(f"report: {checks} checks, {failed} failed")
    return 0 if failed == 0 else 1


sys.exit(main())
