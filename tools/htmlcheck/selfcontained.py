# SPDX-License-Identifier: MIT OR Apache-2.0
import os
import re
import sys

REF = re.compile(r"""\b(?:src|href)\s*=\s*["']([^"']*)["']""", re.I)
CSS = re.compile(r"""url\(\s*["']?([^"')]*)|@import\s+["']?([^"';\s]+)""", re.I)


def external(value):
    return bool(value) and not value.startswith("#") and not value.startswith("data:")


def main(argv):
    if len(argv) != 1:
        print("usage: selfcontained.py HTML_DIR", file=sys.stderr)
        return 2
    files = refs = 0
    for name in sorted(os.listdir(argv[0])) if os.path.isdir(argv[0]) else []:
        if not name.endswith(".html"):
            continue
        files += 1
        text = open(os.path.join(argv[0], name), encoding="utf-8").read()
        found = [v for v in REF.findall(text) if external(v)]
        found += [a or b for a, b in CSS.findall(text) if external(a or b)]
        for v in found:
            print(f"external: {name}: {v}", file=sys.stderr)
        refs += len(found)
    print(f"selfcontained: {files} files, {refs} external references")
    return 0 if files > 0 and refs == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
