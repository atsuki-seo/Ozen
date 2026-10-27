# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402


def projects():
    root = irprops.root_dir()
    for name in sorted(os.listdir(os.path.join(root, "samples"))):
        d = os.path.join(root, "samples", name)
        expect = os.path.join(d, "expect.yaml")
        if not os.path.isfile(expect):
            continue
        entries = sorted(set(re.findall(r"^\s*(?:- )?entry: (\w+)", open(expect, encoding="utf-8").read(), re.M)))
        for entry in entries:
            yield d, entry
    golden = os.path.join(root, "tests", "golden", "bundle")
    for name in sorted(os.listdir(golden)):
        d = os.path.join(golden, name)
        args = os.path.join(d, "args")
        if os.path.isfile(args) and open(args, encoding="utf-8").read().split() == ["bundle"]:
            yield d, None


def run(ozen, cwd, *args):
    return subprocess.run([ozen, *args], cwd=cwd, capture_output=True, text=True)


def declarations(ozen, path):
    out = run(ozen, os.path.dirname(path), "debug", "ast", "--no-spans", os.path.basename(path)).stdout
    blocks, cur = [], None
    for line in out.splitlines()[1:]:
        if line.startswith("  ") and not line.startswith("   "):
            cur = [line]
            blocks.append(cur)
        elif cur is not None and line.startswith("   "):
            cur.append(line)
    return Counter("\n".join(b) for b in blocks if not b[0].startswith("  Import"))


def ozen_files(d):
    for base, _, files in os.walk(d):
        for f in files:
            if f.endswith(".ozen") and f != "package.ozen":
                yield os.path.relpath(os.path.join(base, f), d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        for n, (project, entry) in enumerate(projects()):
            work = os.path.join(tmp, str(n))
            os.makedirs(work)
            bundled = os.path.join(work, "bundle.ozen")
            back = os.path.join(work, "back")
            label = os.path.relpath(project, irprops.root_dir()) + (f" {entry}" if entry else "")
            made = run(ozen, project, "bundle", *([entry] if entry else []), "-o", bundled)
            if made.returncode != 0:
                continue
            cases += 1
            if entry is None:
                entry = re.search(r"^  entry: (\w+)", open(bundled, encoding="utf-8").read(), re.M).group(1)
            restored = run(ozen, work, "unbundle", bundled, "-o", back)
            problems = [] if restored.returncode == 0 else ["unbundle failed"]
            for rel in ozen_files(back) if restored.returncode == 0 else []:
                original = os.path.join(project, rel)
                if not os.path.isfile(original):
                    problems.append(f"{rel} has no original")
                elif declarations(ozen, os.path.join(back, rel)) - declarations(ozen, original):
                    problems.append(f"{rel} declarations differ")
            if restored.returncode == 0:
                a = irprops.ir_lines(ozen, project, None, ["--entry", entry])
                b = irprops.ir_lines(ozen, back, None, ["--entry", entry])
                if not a or a != b:
                    problems.append("IR differs")
            if problems:
                differences += 1
                print(f"difference: {label}: {'; '.join(problems)}", file=sys.stderr)
    print(f"bundle_roundtrip: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
