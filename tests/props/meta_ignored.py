# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402

META = "  meta {\n    inSeason: [winter]\n    density: 9 g/ml\n    unitWeight: 9 g/pcs\n  }\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        for rel, text, flags in irprops.corpus():
            changed = re.sub(r"^(ingredient \w+[^\n{]*\{\n)", lambda m: m.group(1) + META, text, flags=re.M)
            if changed == text:
                continue
            irprops.write(tmp, "a.ozen", text)
            irprops.write(tmp, "b.ozen", changed)
            cases += 1
            if irprops.ir_lines(ozen, tmp, "a.ozen", flags) != irprops.ir_lines(ozen, tmp, "b.ozen", flags):
                differences += 1
                print(f"difference: {rel}", file=sys.stderr)
    print(f"meta_ignored: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
