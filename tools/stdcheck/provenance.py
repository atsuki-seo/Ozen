# SPDX-License-Identifier: MIT OR Apache-2.0
import sys

from common import Defs, quantity, source_of, walk_values


def main(argv):
    if len(argv) != 2:
        print(f"usage: {argv[0]} DEFINITIONS", file=sys.stderr)
        return 2
    defs = Defs(argv[1])
    values = missing = 0
    for d in defs.all:
        if not d["module"].startswith("std.") or d["kind"] not in ("action", "ingredient", "kind", "trait"):
            continue
        for path, value in walk_values(d):
            if quantity(value["text"]) is None and not value["text"].startswith("fct("):
                continue
            if path[-1] == "at" or path[-2:-1] == ["initial"]:
                continue
            values += 1
            if source_of(value) is None and not value["text"].startswith("fct("):
                missing += 1
                print(f"without source: {'.'.join(path)} = {value['text']}", file=sys.stderr)
    print(f"provenance: {values} values, {missing} without source")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
