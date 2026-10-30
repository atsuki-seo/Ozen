# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys

EMPTY_PARENS = re.compile(
    r"\b(?:recipe|menu)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(\s*\)|\bmake\s+[A-Za-z_][A-Za-z0-9_.]*\s*\(\s*\)")
SUFFIXES = (".ozen", ".sh", ".c")


def sources(paths):
    for path in paths:
        if os.path.isdir(path):
            for d, dirs, files in os.walk(path):
                dirs.sort()
                for f in sorted(files):
                    if f.endswith(SUFFIXES):
                        yield os.path.join(d, f)
        elif os.path.isfile(path):
            yield path
        else:
            raise FileNotFoundError(path)


def text_of(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    if not path.endswith(".md"):
        return text
    blocks = re.findall(r"^```ozen\n(.*?)^```", text, re.M | re.S)
    return "\n".join(blocks)


def load_exceptions(path):
    listed, unlisted = set(), []
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            listed.add(os.path.normpath(parts[0]))
        else:
            unlisted.append(parts[0])
    return listed, unlisted


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--empty-parens", action="store_true")
    ap.add_argument("--exceptions", required=True)
    ap.add_argument("paths", nargs="+")
    a = ap.parse_args()
    if not a.empty_parens:
        print("only --empty-parens is available", file=sys.stderr)
        return 2
    try:
        listed, unlisted = load_exceptions(a.exceptions)
        files = list(sources(a.paths))
    except FileNotFoundError as e:
        print(f"no such file: {e}", file=sys.stderr)
        return 2
    found = 0
    for path in files:
        if os.path.normpath(path) in listed:
            continue
        for m in EMPTY_PARENS.finditer(text_of(path)):
            found += 1
            print(f"{path}: {m.group(0)}", file=sys.stderr)
    for path in unlisted:
        print(f"exception without a reason: {path}", file=sys.stderr)
    print(f"empty-parens: {len(files)} sources, {found} found, {len(unlisted)} unlisted exceptions")
    return 0 if found == 0 and not unlisted else 1


sys.exit(main())
