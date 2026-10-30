# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import subprocess
import sys


def rule_names(path):
    text = open(path, encoding="utf-8").read()
    text = re.sub(r"\(\*.*?\*\)", "", text, flags=re.S)
    return re.findall(r"^([a-z_]+)\s*:=", text, re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ap.add_argument("grammar")
    ap.add_argument("golden")
    a = ap.parse_args()
    ozen = os.path.abspath(a.ozen)
    rules = rule_names(a.grammar)
    used = set()
    unknown = set()
    for d, _, files in os.walk(a.golden):
        for f in sorted(files):
            if not f.endswith(".ozen"):
                continue
            out = subprocess.run([ozen, "debug", "ast", "--rules", f], cwd=d, capture_output=True, text=True,
                                 timeout=10).stdout
            for line in out.split("\n"):
                line = line.strip()
                if not line:
                    continue
                if line in rules:
                    used.add(line)
                else:
                    unknown.add(line)
    missing = [r for r in rules if r not in used]
    for r in missing:
        print(f"not covered: {r}", file=sys.stderr)
    for r in sorted(unknown):
        print(f"unknown rule: {r}", file=sys.stderr)
    extra = f", {len(unknown)} unknown" if unknown else ""
    print(f"grammar: {len(rules)} rules, {len(missing)} not covered{extra}")
    return 0 if rules and not missing and not unknown else 1


sys.exit(main())
