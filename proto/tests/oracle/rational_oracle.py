#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import argparse
import os
import random
import re
import shlex
import subprocess
import tempfile
from fractions import Fraction

MAX = 2**63 - 1
CHUNK = 2000


class Overflow(Exception):
    pass


def fits(f):
    return -MAX <= f.numerator <= MAX and 1 <= f.denominator <= MAX


def gen_atom(rng):
    kind = rng.random()
    if kind < 0.35:
        return str(rng.randint(0, 1000))
    if kind < 0.5:
        return str(rng.randint(0, 2**31))
    if kind < 0.7:
        return str(2**62 + rng.randint(-1000, 1000))
    if kind < 0.8:
        return str(MAX - rng.randint(0, 1000))
    if kind < 0.9:
        return f"{rng.randint(0, 999)}.{rng.randint(0, 999):03d}"
    return "0"


def gen(rng, depth):
    if depth == 0 or rng.random() < 0.3:
        atom = gen_atom(rng)
        return f"-{atom}" if rng.random() < 0.15 else atom
    op = rng.choice("+-*/")
    return f"({gen(rng, depth - 1)} {op} {gen(rng, depth - 1)})"


def tokenize(expr):
    return re.findall(r"[0-9]+(?:\.[0-9]+)?|[-+*/()]", expr)


def evaluate(tokens):
    pos = 0
    diags = []

    def atom():
        nonlocal pos
        t = tokens[pos]
        if t == "(":
            pos += 1
            v = expr()
            pos += 1
            return v
        if t == "-":
            pos += 1
            v = atom()
            return None if v is None else -v
        pos += 1
        return Fraction(t)

    def expr():
        nonlocal pos
        left = atom()
        if pos < len(tokens) and tokens[pos] in "+-*/":
            op = tokens[pos]
            pos += 1
            right = atom()
            if left is None or right is None:
                return None
            if op == "/" and right == 0:
                diags.append("MAC002")
                return None
            v = {"+": left + right, "-": left - right, "*": left * right}[op] if op != "/" else left / right
            if not fits(v):
                diags.append("MAC001")
                return None
            return v
        return left

    v = expr()
    return v, diags


def render(f):
    return str(f.numerator) if f.denominator == 1 else f"{f.numerator}/{f.denominator}"


def run_chunk(runner, exprs):
    with tempfile.TemporaryDirectory(prefix="ozen-oracle-") as tmp:
        path = os.path.join(tmp, "oracle.ozen")
        with open(path, "w", encoding="utf-8") as f:
            f.write("action show(x)\nrecipe r() {\n")
            for e in exprs:
                f.write(f"  show({e})\n")
            f.write("}\n")
        r = subprocess.run(shlex.split(runner) + ["--limit-input", "10000000", path], capture_output=True, text=True)
    steps = []
    diags = {}
    for line in r.stdout.splitlines():
        m = re.match(r"^%[0-9]+ = show\((.*)\)$", line)
        if m:
            steps.append(m.group(1))
            continue
        m = re.match(r"^error (\S+) \S+ ([0-9]+):", line)
        if m:
            diags.setdefault(int(m.group(2)), []).append(m.group(1))
    return steps, diags


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--runner", required=True)
    ap.add_argument("--cases", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args(argv)
    rng = random.Random(args.seed)
    mismatches = 0
    done = 0
    while done < args.cases:
        n = min(CHUNK, args.cases - done)
        exprs = [gen(rng, 3) for _ in range(n)]
        steps, diags = run_chunk(args.runner, exprs)
        si = 0
        for i, e in enumerate(exprs):
            want, want_diags = evaluate(tokenize(e))
            line = i + 3
            got_diags = sorted(diags.get(line, []))
            if want is None:
                ok = got_diags == sorted(want_diags)
            else:
                got = steps[si] if si < len(steps) else None
                si += 1
                ok = not got_diags and got == render(want)
            if not ok:
                mismatches += 1
                if mismatches <= 5:
                    print(f"mismatch: {e}: want {want if want is None else render(want)} {want_diags}, "
                          f"got diags {got_diags}", file=sys.stderr)
        done += n
    print(f"oracle: {args.cases} cases, {mismatches} mismatches")
    return 0 if mismatches == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
