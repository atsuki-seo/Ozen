#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import glob
import html
import os
import re
import subprocess
import sys
import tempfile

ITEM = re.compile(r'<span class="file"[^>]*>(.*?)</span>|<pre([^>]*)>(.*?)</pre>', re.S)
FILE = re.compile(r"([A-Za-z0-9_.-]+\.ozen)")
VARIANT = "にすると"
VARIANT_ARGS = ["debug", "steps"]
LAST_LET = re.compile(r"^(\s*let [A-Za-z0-9_]+ = ).*$", re.M)


def text_of(fragment):
    return html.unescape(re.sub(r"<[^>]*>", "", fragment))


def lines_tokens(text):
    return [l.split() for l in text.split("\n") if l.strip()]


def contains_in_order(shown, source):
    flat = [t for line in lines_tokens(source) for t in line]
    at = 0
    for want in lines_tokens(shown):
        n = len(want)
        while at + n <= len(flat) and flat[at:at + n] != want:
            at += 1
        if at + n > len(flat):
            return False
        at += n
    return True


def normalize_output(text):
    out = []
    for line in text.split("\n"):
        if re.match(r"^exit \d+$", line):
            continue
        line = re.sub(r"\S+\.ozen:(?=\d+:\d+)", "", line)
        line = re.sub(r"(\]|\S): [a-z][a-z ]*$", r"\1", line)
        out.append(line)
    return "\n".join(out)


def strip_args_line(text):
    first, _, rest = text.partition("\n")
    return rest if first.startswith("// args: ") else text


def args_line(text):
    first = text.split("\n", 1)[0]
    return first[len("// args: "):].split() if first.startswith("// args: ") else ["check"]


def source_of(root, name):
    hits = sorted(glob.glob(os.path.join(root, "tests/golden/**", name), recursive=True))
    hits += sorted(glob.glob(os.path.join(root, "samples/**", name), recursive=True))
    return hits


def run_ozen(ozen, args, name, code):
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, name), "w", encoding="utf-8") as f:
            f.write(code + "\n")
        p = subprocess.run([ozen, *args, name], cwd=d, capture_output=True, text=True)
        return p.stdout + p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ap.add_argument("--root", default=".")
    ap.add_argument("page")
    a = ap.parse_args()
    ozen = os.path.abspath(a.ozen)
    page = open(a.page, encoding="utf-8").read()

    examples, mismatched, unmapped = 0, 0, 0
    known = {}
    last = None
    label = None
    for m in ITEM.finditer(page):
        if m.group(1) is not None:
            label = text_of(m.group(1)).strip()
            continue
        attrs, body = m.group(2), text_of(m.group(3))
        here, label = label, None
        examples += 1
        if 'class="out"' not in attrs:
            fm = FILE.search(here or "")
            if not fm:
                unmapped += 1
                print(f"unmapped code: {body.splitlines()[0] if body else ''}", file=sys.stderr)
                last = None
                continue
            name = fm.group(1)
            hits = source_of(a.root, name)
            if len(hits) == 1:
                src = open(hits[0], encoding="utf-8").read()
                ex = {"name": name, "golden": hits[0], "code": strip_args_line(src), "args": args_line(src)}
                ok = contains_in_order(body, ex["code"])
            elif not hits and "抜粋" not in here:
                # The page wraps "-> T<...>" onto its own line to fit its columns; the parser
                # needs it on the line of the declaration, so the source joins it back.
                ex = {"name": name, "golden": None, "code": re.sub(r"\n\s*->", " ->", body), "args": ["check"]}
                ok = True
            else:
                unmapped += 1
                print(f"unmapped code: {name} ({len(hits)} sources)", file=sys.stderr)
                last = None
                continue
            known[name] = ex
            last = ex
            if not ok:
                mismatched += 1
                print(f"mismatched code: {name}", file=sys.stderr)
            continue

        ex, args, code = last, None, None
        if here and here.endswith(VARIANT) and last is not None:
            expr = here[:-len(VARIANT)].strip()
            lets = list(LAST_LET.finditer(last["code"]))
            if last["golden"] is None and lets:
                l = lets[-1]
                code = last["code"][:l.start()] + l.group(1) + expr + last["code"][l.end():]
                args = VARIANT_ARGS
            else:
                ex = None
        elif here and here.startswith("ozen "):
            words = here.split()[1:]
            fm = [w for w in words if w.endswith(".ozen")]
            ex = known.get(fm[-1]) if fm else None
            args = [w for w in words if not w.endswith(".ozen")]
        if ex is None:
            unmapped += 1
            print(f"unmapped output: {here or body.splitlines()[0]}", file=sys.stderr)
            continue
        if ex["golden"] is not None:
            ok = args is None or args == ex["args"]
            actual = open(ex["golden"][:-len(".ozen")] + ".out", encoding="utf-8").read()
        else:
            ok = True
            actual = run_ozen(ozen, args or ex["args"], ex["name"], code or ex["code"])
        if not (ok and contains_in_order(body, normalize_output(actual))):
            mismatched += 1
            print(f"mismatched output: {here or ex['name']}", file=sys.stderr)

    print(f"site-examples: {examples} examples, {mismatched} mismatched, {unmapped} unmapped")
    return 0 if examples > 0 and mismatched == 0 and unmapped == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
