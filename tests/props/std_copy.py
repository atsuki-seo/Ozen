# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402

PACKAGE = 'package book {\n  version: "1.0.0"\n}\n'
TOP = re.compile(r"^(action|ingredient) (\w+)")


def std_definitions():
    defs = {}
    path = os.path.join(irprops.root_dir(), "stdlib", "prelude.ozen")
    lines = open(path, encoding="utf-8").read().splitlines()
    start = None
    for i, line in enumerate(lines + [""]):
        top = line[:1] not in ("", " ", "}")
        if top or i == len(lines):
            if start is not None:
                m = TOP.match(lines[start])
                if m:
                    defs.setdefault(m.group(1), {})[m.group(2)] = "\n".join(lines[start:i]).rstrip() + "\n"
            start = i if top else None
    return defs


def used(text, defs):
    calls = {m.group(1) for m in re.finditer(r"\b([a-z]\w*)\(", text)}
    local = {m.group(2) for m in re.finditer(r"^(action|ingredient) (\w+)", text, re.M)}
    return [defs["action"][n] for n in sorted(calls) if n in defs.get("action", {}) and n not in local]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    defs = std_definitions()
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        plain, copied = os.path.join(tmp, "plain"), os.path.join(tmp, "copied")
        for rel, text, flags in irprops.corpus():
            copies = used(text, defs)
            if not copies:
                continue
            for d in (plain, copied):
                os.makedirs(d, exist_ok=True)
                for f in os.listdir(d):
                    os.remove(os.path.join(d, f))
                irprops.write(d, "package.ozen", PACKAGE)
            irprops.write(plain, "main.ozen", text)
            irprops.write(copied, "main.ozen", "".join(copies) + text)
            a = irprops.ir_lines(ozen, plain, None, flags)
            b = irprops.ir_lines(ozen, copied, None, flags)
            if not a:
                continue
            cases += 1
            if a != b:
                differences += 1
                print(f"difference: {rel}", file=sys.stderr)
    print(f"std_copy: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
