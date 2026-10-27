# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402


def reverse_ingredients(text):
    def one_line(m):
        items = [i.strip() for i in m.group(2).split(",")]
        return m.group(1) + ", ".join(reversed(items)) + m.group(3)

    text = re.sub(r"(ingredients \{ )([^{}\n]*,[^{}\n]*)( \})", one_line, text)
    lines, out, k = text.split("\n"), [], 0
    while k < len(lines):
        out.append(lines[k])
        if lines[k].rstrip().endswith("ingredients {"):
            indent = len(lines[k]) - len(lines[k].lstrip())
            j = k + 1
            while j < len(lines) and not (lines[j].strip() == "}" and len(lines[j]) - len(lines[j].lstrip()) == indent):
                j += 1
            out += list(reversed(lines[k + 1:j]))
            k = j
            continue
        k += 1
    return "\n".join(out)


def rename_waves(text):
    names = set(re.findall(r"^\s*serve (\w+)", text, re.M))
    for n in names:
        text = re.sub(rf"(\bserve |\bafter ){n}\b", rf"\g<1>shown{n.capitalize()}", text)
    return text


def strip_groups(text):
    return re.sub(r" @group\(\w+\)", "", text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        for rel, text, flags in irprops.corpus():
            changed = strip_groups(rename_waves(reverse_ingredients(text)))
            if changed == text:
                continue
            irprops.write(tmp, "a.ozen", text)
            irprops.write(tmp, "b.ozen", changed)
            a = irprops.ir_lines(ozen, tmp, "a.ozen", flags)
            b = irprops.ir_lines(ozen, tmp, "b.ozen", flags)
            cases += 1
            if a != b:
                differences += 1
                print(f"difference: {rel}", file=sys.stderr)
    print(f"display_only: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
