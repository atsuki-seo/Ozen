#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
import sys


def read_pins(path):
    pins = {}
    for line in open(path, encoding="utf-8"):
        parts = line.split()
        if not parts or parts[0].startswith("#") or len(parts) < 2:
            continue
        name, version, tag = parts[0], parts[1], (parts[2] if len(parts) > 2 else "")
        if name not in pins or tag == "ci":
            pins[name] = version
    return pins


def main(argv):
    if len(argv) < 2:
        print("usage: check-ci-tools.py TOOLCHAIN TOOLS.txt...", file=sys.stderr)
        return 2
    pins = read_pins(argv[0])
    mismatched = 0
    for path in argv[1:]:
        job = os.path.basename(os.path.dirname(os.path.abspath(path))).removeprefix("tools-")
        try:
            lines = [line.split() for line in open(path, encoding="utf-8") if line.strip()]
        except OSError:
            lines = []
        if not lines:
            print(f"mismatch: {job}: no tools recorded", file=sys.stderr)
            mismatched += 1
        for parts in lines:
            name, got = parts[0], (parts[1] if len(parts) > 1 else "")
            want = pins.get(name)
            if want is None:
                print(f"mismatch: {job}: {name} {got} is not pinned", file=sys.stderr)
                mismatched += 1
            elif got != want:
                print(f"mismatch: {job}: {name} {got} (want {want})", file=sys.stderr)
                mismatched += 1
    print(f"ci-tools: {len(argv) - 1} jobs, {mismatched} mismatched")
    return 0 if mismatched == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
