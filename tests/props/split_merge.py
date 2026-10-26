# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402

PACKAGE = 'package book {\n  version: "1.0.0"\n}\n'
IMPORTABLE = r"^(action|ingredient|alias|trait|kind|vessel|type|macro|property|unit|axis|enum|tool|resource|dirt) (\w+)"


def last_recipe(text):
    starts = [m.start() for m in re.finditer(r"^recipe (\w+)", text, re.M)]
    if not starts:
        return None
    start = starts[-1]
    end = text.find("\n}\n", start)
    if end < 0:
        return None
    end += 3
    name = re.match(r"recipe (\w+)", text[start:]).group(1)
    return name, text[:start] + text[end:], text[start:end]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        merged, split = os.path.join(tmp, "merged"), os.path.join(tmp, "split")
        for rel, text, flags in irprops.corpus():
            found = last_recipe(text)
            if found is None or flags:
                continue
            name, rest, dish = found
            names = sorted(set(m.group(2) for m in re.finditer(IMPORTABLE, rest, re.M)))
            for d in (merged, split):
                os.makedirs(d, exist_ok=True)
                for f in os.listdir(d):
                    os.remove(os.path.join(d, f))
                irprops.write(d, "package.ozen", PACKAGE)
            irprops.write(merged, "main.ozen", text)
            irprops.write(split, "main.ozen", rest)
            header = f"import book.main.{{{', '.join(names)}}}\n" if names else ""
            irprops.write(split, "dish.ozen", header + dish)
            a = irprops.ir_lines(ozen, merged, None, ["--entry", name])
            b = irprops.ir_lines(ozen, split, None, ["--entry", name])
            if not a:
                continue
            cases += 1
            if a != b:
                differences += 1
                print(f"difference: {rel}", file=sys.stderr)
    print(f"split_merge: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
