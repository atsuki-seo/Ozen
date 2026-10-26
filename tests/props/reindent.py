# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import random
import subprocess
import sys


def whitespace_runs(src):
    """Yield (start, end) of space/tab runs outside strings and comments."""
    i, n = 0, len(src)
    while i < n:
        if src.startswith('"""', i):
            j = src.find('"""', i + 3)
            i = n if j < 0 else j + 3
        elif src[i] == '"':
            i += 1
            while i < n and src[i] not in '"\n':
                i += 2 if src[i] == "\\" else 1
            i += 1
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif src[i] in " \t":
            j = i
            while j < n and src[j] in " \t":
                j += 1
            yield i, j
            i = j
        else:
            i += 1


def respace(src, pick):
    out, last = [], 0
    for s, e in whitespace_runs(src):
        out.append(src[last:s])
        out.append(pick(s, e, src))
        last = e
    out.append(src[last:])
    return "".join(out)


def at_line_start(s, src):
    return s == 0 or src[s - 1] == "\n"


def variants(src, seed):
    rng = random.Random(seed)
    yield respace(src, lambda s, e, t: "" if at_line_start(s, t) else t[s:e])
    yield respace(src, lambda s, e, t: "\t" * (e - s) if at_line_start(s, t) else t[s:e])
    yield respace(src, lambda s, e, t: "".join(rng.choice(" \t") for _ in range(rng.randint(1, 4))))
    yield add_trailing(src)


def add_trailing(src):
    inside = set()
    i = 0
    while True:
        s = src.find('"""', i)
        if s < 0:
            break
        e = src.find('"""', s + 3)
        e = len(src) if e < 0 else e + 3
        inside.update(range(s, e))
        i = e
    return "".join(" \t\n" if ch == "\n" and k not in inside else ch for k, ch in enumerate(src))


def dump(ozen, path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    r = subprocess.run([ozen, "debug", "ast", "--no-spans", os.path.basename(path)], cwd=os.path.dirname(path),
                       capture_output=True, timeout=10)
    return r.returncode, r.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ap.add_argument("--golden", default="tests/golden")
    ap.add_argument("--work", default="build/props/reindent")
    a = ap.parse_args()
    ozen = os.path.abspath(a.ozen)
    os.makedirs(a.work, exist_ok=True)
    cases = diffs = 0
    for d, _, files in os.walk(a.golden):
        for f in sorted(files):
            if not f.endswith(".ozen"):
                continue
            path = os.path.join(d, f)
            try:
                src = open(path, encoding="utf-8").read()
            except UnicodeDecodeError:
                continue
            base = os.path.join(os.path.abspath(a.work), f)
            status, want = dump(ozen, base, src)
            if status != 0:
                continue
            for k, v in enumerate(variants(src, f)):
                cases += 1
                vs, got = dump(ozen, base, v)
                if vs != status or got != want:
                    diffs += 1
                    print(f"difference: {path} variant {k}", file=sys.stderr)
    print(f"reindent: {cases} cases, {diffs} differences")
    return 0 if cases > 0 and diffs == 0 else 1


sys.exit(main())
