# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import fnmatch
import os
import re
import shutil
import subprocess
import sys
import tempfile

BUILD = ["scripts/build-core.sh", "--only", "core-gcc-release"]
BINARY = "build/core-gcc-release/ozen"
CODE = re.compile(r"^(?:error|warning|info) ([A-Z]{3}[0-9]{3})\b|\"code\":\"([A-Z]{3}[0-9]{3})\"", re.M)
RESULT = re.compile(r"^result \w+ errors=(\d+) warnings=(\d+) infos=(\d+)$|"
                    r"\"errors\":(\d+),\"warnings\":(\d+),\"infos\":(\d+)", re.M)
STEP = re.compile(r"%\d+ = (?:op |choose )?(?:make |buy )?([A-Za-z_][A-Za-z0-9_]*)|"
                  r"^\s+[0-9]+\.\.[0-9]+ \([0-9]+\) %\d+ ([A-Za-z_][A-Za-z0-9_]*)", re.M)
ROW_NAME = re.compile(r"`((?:cgolden|cli|cunit)/[^`\s]+)`")


def git(*args, cwd=None):
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=True).stdout


def changed_files(base, head):
    return [f for f in git("diff", "--name-only", "--no-renames", base, head).split("\n") if f]


def golden_entries(ref, root):
    files = [f for f in git("ls-tree", "-r", "--name-only", ref, "--", root).split("\n") if f]
    folders = {os.path.dirname(f) for f in files if os.path.basename(f) == "expected.out"}
    entries = set(folders)
    for f in files:
        if f.endswith(".ozen") and not any(f.startswith(d + "/") for d in folders):
            entries.add(f[:-5])
    return entries, folders


def entry_of(path, entries, folders):
    for d in folders:
        if path.startswith(d + "/"):
            return d
    if path.endswith((".ozen", ".out")) and path[:path.rfind(".")] in entries:
        return path[:path.rfind(".")]
    return None


def build(ref, cache):
    sha = git("rev-parse", "--verify", ref + "^{commit}").strip()
    binary = os.path.join(cache, sha, "ozen")
    if os.path.exists(binary):
        return sha, binary
    tmp = tempfile.mkdtemp()
    wt = os.path.join(tmp, "wt")
    try:
        git("worktree", "add", "--detach", wt, sha)
        subprocess.run(BUILD, cwd=wt, capture_output=True, check=True)
        os.makedirs(os.path.dirname(binary), exist_ok=True)
        shutil.copy2(os.path.join(wt, BINARY), binary)
    finally:
        subprocess.run(["git", "worktree", "remove", "--force", wt], capture_output=True)
        shutil.rmtree(tmp, ignore_errors=True)
    return sha, binary


def run(binary, sha, entry, folder):
    tmp = tempfile.mkdtemp()
    try:
        if folder:
            names = [f for f in git("ls-tree", "-r", "--name-only", sha, "--", entry).split("\n") if f]
        else:
            names = [entry + ".ozen"]
        for name in names:
            dest = os.path.join(tmp, name)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as f:
                f.write(subprocess.run(["git", "show", f"{sha}:{name}"], capture_output=True, check=True).stdout)
        if folder:
            rundir = os.path.join(tmp, entry)
            args_file = os.path.join(rundir, "args")
            args = open(args_file, encoding="utf-8").read().split() if os.path.exists(args_file) else ["check"]
            target = []
        else:
            rundir = os.path.join(tmp, os.path.dirname(entry))
            first = open(os.path.join(tmp, entry + ".ozen"), encoding="utf-8", errors="replace").readline()
            args = first[len("// args: "):].split() if first.startswith("// args: ") else ["check"]
            target = [os.path.basename(entry) + ".ozen"]
        p = subprocess.run([binary, *args, *target], cwd=rundir, capture_output=True, timeout=30)
        return p.returncode, p.stdout.decode("utf-8", "replace")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def features(status, out):
    codes = [a or b for a, b in CODE.findall(out)]
    results = [tuple(x for x in m if x) for m in RESULT.findall(out)]
    steps = [a or b for a, b in STEP.findall(out)]
    return (status, codes, results), steps


def section(path, start, end):
    out, inside = [], False
    for line in open(path, encoding="utf-8"):
        if not inside and re.match(start, line):
            inside = True
            continue
        if inside and re.match(end, line):
            break
        if inside:
            out.append(line.rstrip("\n"))
    return out


def roadmap_rows(path):
    rows = []
    for line in section(path, r"^### 5\.3", r"^## 6\."):
        if not line.startswith("|") or re.match(r"^\|\s*-", line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 4 and re.match(r"^S\d+$", cells[0]) and cells[3] not in ("", "—", "-"):
            rows.append(cells)
    return rows


def row_matches(name, names, unit_text):
    if name.startswith("cunit/"):
        pattern = name[len("cunit/"):]
        return any(re.search(r"\b%s\b" % re.escape(pattern).replace(r"\*", r"\w*"), t) for t in unit_text)
    return any(fnmatch.fnmatchcase(n, name) for n in names)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--head", required=True)
    ap.add_argument("--records")
    ap.add_argument("--roadmap", default="plans/ozen-roadmap.md")
    ap.add_argument("--cache", default="build/equivalence")
    ap.add_argument("golden")
    a = ap.parse_args()
    root = a.golden.rstrip("/")
    cache = os.path.abspath(a.cache)
    try:
        changed = changed_files(a.base, a.head)
        head_entries, folders = golden_entries(a.head, root)
        entries = head_entries & golden_entries(a.base, root)[0]
    except subprocess.CalledProcessError as e:
        print(e.stderr.strip(), file=sys.stderr)
        return 2
    rewritten = sorted({e for e in (entry_of(f, entries, folders) for f in changed) if e})
    changed_results, changed_steps = [], []
    if rewritten:
        base_sha, base_bin = build(a.base, cache)
        head_sha, head_bin = build(a.head, cache)
        for e in rewritten:
            folder = e in folders
            before, before_steps = features(*run(base_bin, base_sha, e, folder))
            after, after_steps = features(*run(head_bin, head_sha, e, folder))
            if before != after:
                changed_results.append(e)
                print(f"changed result: {e}: {before} -> {after}", file=sys.stderr)
            elif before_steps != after_steps:
                changed_steps.append(e)
                print(f"changed steps: {e}: {before_steps} -> {after_steps}", file=sys.stderr)
    golden_names = {os.path.relpath(e, root) for e in rewritten}
    cli_names = {os.path.basename(f)[:-3] for f in changed if f.startswith("tests/cli/") and f.endswith(".sh")}
    unit_text = [subprocess.run(["git", "show", f"{a.head}:{f}"], capture_output=True, text=True).stdout
                 for f in changed if f.startswith("core/tests/unit/") and f.endswith(".c")]
    names = {"cgolden/" + g for g in golden_names} | {"cli/" + c for c in cli_names}
    rows = []
    for cells in roadmap_rows(a.roadmap):
        hits = [n for n in ROW_NAME.findall(cells[2]) if row_matches(n, names, unit_text)]
        if hits:
            rows.append((cells[0], cells[1], hits))
    records = []
    if a.records:
        records = [l for l in section(a.records, r"^## 9\.", r"^## 10\.")
                   if re.match(r"^\s*- 書き直しのわざと壊す確認: ", l)]
    missing = 0
    for s, view, hits in rows:
        if not any(re.search(r"\b%s\b" % s, r) and view in r and any(f"`{h}`" in r for h in hits) for r in records):
            missing += 1
            print(f"missing record: {s} {view} {' '.join(hits)}", file=sys.stderr)
    print(f"equivalence: {len(rewritten)} rewritten, {len(changed_results)} changed results, "
          f"{len(changed_steps)} changed steps; records: {len(rows)} roadmap rows, {missing} missing")
    return 0 if not changed_results and missing == 0 else 1


sys.exit(main())
