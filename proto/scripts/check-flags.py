#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import argparse
import json
import os
import shlex
import sys

sys.dont_write_bytecode = True

WARNINGS = ["-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", "-Wstrict-prototypes", "-Werror"]


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--std-gcc", required=True)
    ap.add_argument("--std-clang", required=True)
    ap.add_argument("--extra", action="append", default=[])
    ap.add_argument("compile_commands")
    argv = list(sys.argv[1:] if argv is None else argv)
    joined = []
    while argv:
        a = argv.pop(0)
        if a == "--extra" and argv:
            joined.append("--extra=" + argv.pop(0))
        else:
            joined.append(a)
    args = ap.parse_args(joined)
    with open(args.compile_commands, encoding="utf-8") as f:
        entries = json.load(f)
    files = missing = 0
    for e in entries:
        path = e["file"].replace("\\", "/")
        if "/src/" not in path or not path.endswith(".c"):
            continue
        files += 1
        words = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        cc = os.path.basename(words[0])
        std = args.std_clang if "clang" in cc else args.std_gcc
        want = WARNINGS + [f"-std={std}"] + args.extra
        lacking = [w for w in want if w not in words]
        if lacking:
            missing += 1
            print(f"missing in {path}: {' '.join(lacking)}", file=sys.stderr)
    print(f"flags: {files} files, {missing} missing")
    return 0 if files and not missing else 1


if __name__ == "__main__":
    sys.exit(main())
