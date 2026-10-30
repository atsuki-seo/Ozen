# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys

EMPTY_PARENS = re.compile(
    r"\b(?:recipe|menu)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(\s*\)|\bmake\s+[A-Za-z_][A-Za-z0-9_.]*\s*\(\s*\)")
DECLARATION = re.compile(r"\b(?:recipe|menu)\s+([A-Za-z_][A-Za-z0-9_]*)")
BINDING = re.compile(r"\blet\s+(?:([A-Za-z_][A-Za-z0-9_]*)|\(([^)]*)\))")
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


def without_comments(text):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)


def samples_pipe(paths, listed, unlisted):
    samples = sorted(os.path.join(p, d) for p in paths for d in os.listdir(p) if os.path.isdir(os.path.join(p, d)))
    missing = 0
    for sample in samples:
        if os.path.normpath(sample) in listed:
            continue
        texts = [without_comments(open(f, encoding="utf-8", errors="replace").read()) for f in sources([sample])
                 if f.endswith(".ozen")]
        if not any("|>" in t for t in texts):
            missing += 1
            print(f"{sample}: no pipe", file=sys.stderr)
    for path in unlisted:
        print(f"exception without a reason: {path}", file=sys.stderr)
    print(f"samples-pipe: {len(samples)} samples, {missing} without pipe, {len(unlisted)} unlisted exceptions")
    return 0 if missing == 0 and not unlisted else 1


def names(files, listed, unlisted):
    declarations = bindings = short = 0
    for path in files:
        if os.path.normpath(path) in listed:
            continue
        text = text_of(path)
        found = DECLARATION.findall(text)
        bound = []
        for single, group in BINDING.findall(text):
            bound += [single] if single else [n.strip() for n in group.split(",") if n.strip()]
        declarations += len(found)
        bindings += len(bound)
        for name in found + bound:
            if len(name) == 1:
                short += 1
                print(f"{path}: {name}", file=sys.stderr)
    for path in unlisted:
        print(f"exception without a reason: {path}", file=sys.stderr)
    print(f"names: {declarations} declarations, {bindings} bindings, {short} one-letter, "
          f"{len(unlisted)} unlisted exceptions")
    return 0 if short == 0 and not unlisted else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--empty-parens", action="store_true")
    ap.add_argument("--samples-pipe", action="store_true")
    ap.add_argument("--exceptions", required=True)
    ap.add_argument("paths", nargs="+")
    a = ap.parse_args()
    try:
        listed, unlisted = load_exceptions(a.exceptions)
        if a.samples_pipe:
            return samples_pipe(a.paths, listed, unlisted)
        files = list(sources(a.paths))
    except FileNotFoundError as e:
        print(f"no such file: {e}", file=sys.stderr)
        return 2
    if not a.empty_parens:
        return names(files, listed, unlisted)
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
