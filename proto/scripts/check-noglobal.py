#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import os
import re
import subprocess

WRITABLE = re.compile(r"^\.(data|bss|tdata|tbss)(\..*)?$")


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} LIBRARY...", file=sys.stderr)
        return 2
    objects = 0
    bad = 0
    for lib in argv[1:]:
        r = subprocess.run(["objdump", "-h", lib], capture_output=True, text=True, env=dict(os.environ, LC_ALL="C"))
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
            return 2
        current = None
        for line in r.stdout.splitlines():
            m = re.match(r"^(\S+\.o):\s+file format", line)
            if m:
                current = m.group(1)
                objects += 1
                continue
            m = re.match(r"^\s*[0-9]+\s+(\S+)\s+([0-9a-f]+)\s", line)
            if not m:
                continue
            name, size = m.group(1), int(m.group(2), 16)
            if name.startswith(".data.rel.ro"):
                continue
            if WRITABLE.match(name) and size > 0:
                bad += 1
                print(f"writable section: {lib}: {current}: {name} ({size} bytes)", file=sys.stderr)
    print(f"noglobal: {objects} objects, {bad} writable sections")
    return 0 if objects and not bad else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
