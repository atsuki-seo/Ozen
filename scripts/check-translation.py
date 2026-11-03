#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import argparse
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
JAPANESE = re.compile(r"[　-〿぀-ヿ㐀-䶿一-鿿＀-￯]")
ID = re.compile(r"(?<![A-Za-z0-9-])(D[0-9]+|S[0-9]+|P[0-9]+-[0-9]+|R[0-9]+|G-[A-Z0-9]+(?:-[A-Z0-9]+)*)(?![A-Za-z0-9])")
HEADING = re.compile(r"^(#{1,6})\s+(.*)$")
NUMBER = re.compile(r"^(?:[0-9]+(?:\.[0-9]+)*\.?|[A-Z]+[0-9]+(?:-[0-9]+)?\.?)$")
SHELL = {"bash", "sh", "shell"}
REMOVED_SECTIONS = {"docs/concept.md": ["16."]}


def git_show(rev, path):
    r = subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"error: cannot read {path} at {rev}")
    return r.stdout


def read_labels():
    pairs = []
    for row in open(os.path.join(HERE, "plan-labels.tsv"), encoding="utf-8").read().split("\n"):
        if row:
            ja, en = row.split("\t")
            pairs.append((ja, en))
    return pairs


def forms(ja, en):
    out = [(ja, en), (ja.rstrip(" "), en.rstrip(" ")), (ja.rstrip(" *"), en.rstrip(" *"))]
    core_ja = ja.strip(" ").lstrip("- ").strip("*").rstrip(": ").strip()
    core_en = en.strip(" ").lstrip("- ").strip("*").rstrip(": ").strip()
    if len(core_ja) >= 4 and not any(placeholder(core_ja, i) for i in range(len(core_ja))):
        out.append((core_ja, core_en))
    return out


def placeholder(text, i):
    return (text[i] in "NM" and (i == 0 or not text[i - 1].isalnum())
            and (i + 1 == len(text) or not text[i + 1].isalnum()))


def tolerant(text):
    parts = []
    for i, ch in enumerate(text):
        if placeholder(text, i):
            parts.append(r"(\S+?)")
        elif ch in r"[](){}.*+?|^$":
            parts.append(r"\\?" + re.escape(ch))
        else:
            parts.append(re.escape(ch))
    return "".join(parts)


def label_rules(pairs):
    rules = []
    for ja, en in pairs:
        for fj, fe in forms(ja, en):
            if not fj:
                continue
            groups = 0
            repl = []
            for i, ch in enumerate(fe):
                if placeholder(fe, i):
                    groups += 1
                    repl.append("\\%d" % groups)
                else:
                    repl.append(ch.replace("\\", "\\\\"))
            rules.append((len(fj), re.compile(tolerant(fj)), "".join(repl), re.compile(tolerant(fe)), fe))
    rules.sort(key=lambda r: -r[0])
    return rules


def apply_map(text, rules):
    for _, ja_re, repl, _, _ in rules:
        text = ja_re.sub(repl, text)
    return text


def canonical(text, rules):
    for _, _, _, en_re, fe in rules:
        if any(placeholder(fe, i) for i in range(len(fe))):
            continue
        text = en_re.sub(lambda m: fe, text)
    return text


def drop_sections(text, numbers):
    lines, out, skip_level = text.split("\n"), [], None
    for line in lines:
        m = HEADING.match(line)
        if m:
            level = len(m.group(1))
            if skip_level is not None and level <= skip_level:
                skip_level = None
            token = m.group(2).split(" ", 1)[0]
            if skip_level is None and token in numbers:
                skip_level = level
        if skip_level is None:
            out.append(line)
    return "\n".join(out)


def split(text):
    """Return prose lines with code removed, code blocks, inline spans per section, headings, table rows per section."""
    prose, blocks, headings = [], [], []
    spans = [collections.Counter()]
    rows = [0]
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        fence = re.match(r"^(\s*)(`{3,}|~{3,})(.*)$", line)
        if fence:
            mark, lang = fence.group(2), fence.group(3).strip().split(" ")[0]
            body = []
            i += 1
            while i < len(lines) and not lines[i].lstrip().startswith(mark):
                body.append(lines[i])
                i += 1
            blocks.append((lang, body))
            i += 1
            continue
        m = HEADING.match(line)
        if m:
            token = m.group(2).split(" ", 1)[0]
            headings.append((len(m.group(1)), token if NUMBER.match(token) else "-"))
            spans.append(collections.Counter())
            rows.append(0)
        if line.lstrip().startswith("|") and not re.match(r"^\s*\|[\s:|-]*$", line):
            rows[-1] += 1
        out, j = [], 0
        while j < len(line):
            if line[j] == "`":
                k = j
                while k < len(line) and line[k] == "`":
                    k += 1
                n = k - j
                end = re.search(r"(?<!`)`{%d}(?!`)" % n, line[k:])
                if end:
                    spans[-1][line[k:k + end.start()].replace("\\|", "|")] += 1
                    out.append(" ")
                    j = k + end.end()
                    continue
                out.append(line[j:k])
                j = k
                continue
            out.append(line[j])
            j += 1
        prose.append("".join(out))
        i += 1
    return prose, blocks, spans, headings, rows


def strip_shell_comments(body):
    out = []
    for line in body:
        if re.match(r"^\s*#", line):
            continue
        out.append(re.sub(r"\s+#\s.*$|\s+#$", "", line))
    return out


def shell_comments(body):
    out = []
    for line in body:
        m = re.match(r"^\s*(#.*)$", line) or re.search(r"\s(#\s.*)$", line)
        if m:
            out.append(m.group(1))
    return out


def read_allow(path, problems):
    allow = []
    if not os.path.exists(path):
        return allow
    for n, row in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        if not row:
            continue
        if "\t" not in row or not row.split("\t", 1)[1].strip():
            problems.append(f"{path}:{n}: entry without a reason")
            continue
        allow.append(row.split("\t", 1)[0])
    return allow


def check_file(path, base, rules, allow, problems):
    new = open(path, encoding="utf-8").read()
    old = git_show(base, path)
    old = drop_sections(old, REMOVED_SECTIONS.get(path, []))
    before = len(problems)
    if set(ID.findall(old)) != set(ID.findall(new)):
        a, b = set(ID.findall(old)), set(ID.findall(new))
        problems.append(f"{path}: IDs differ: missing {sorted(a - b)[:10]}, added {sorted(b - a)[:10]}")
    p_old, b_old, s_old, h_old, r_old = split(old)
    p_new, b_new, s_new, h_new, r_new = split(new)
    if h_old != h_new:
        for k, (x, y) in enumerate(zip(h_old + [None] * len(h_new), h_new + [None] * len(h_old))):
            if x != y:
                problems.append(f"{path}: heading {k + 1} differs: {x} -> {y}")
                break
    elif r_old != r_new:
        for k, (x, y) in enumerate(zip(r_old, r_new)):
            if x != y:
                problems.append(f"{path}: section {k} has {y} table rows, was {x}")
    if len(b_old) != len(b_new):
        problems.append(f"{path}: {len(b_new)} code blocks, was {len(b_old)}")
    for k, ((lo, bo), (ln, bn)) in enumerate(zip(b_old, b_new)):
        if lo in SHELL:
            bo, bn = strip_shell_comments(bo), strip_shell_comments(bn)
            for c in shell_comments(b_new[k][1]):
                if JAPANESE.search(c):
                    problems.append(f"{path}: Japanese in a shell comment of code block {k + 1}: {c[:60]}")
        mo = canonical(apply_map("\n".join(bo), rules), rules)
        mn = canonical("\n".join(bn), rules)
        if lo != ln or mo != mn:
            ol, nl = mo.split("\n"), mn.split("\n")
            diff = next((f"{a!r} -> {b!r}" for a, b in zip(ol + [""], nl + [""]) if a != b), "")
            problems.append(f"{path}: code block {k + 1} differs: {diff[:160]}")
    if h_old == h_new:
        for k, (so, sn) in enumerate(zip(s_old, s_new)):
            mo = collections.Counter(canonical(apply_map(s, rules), rules) for s in so.elements())
            mn = collections.Counter(canonical(s, rules) for s in sn.elements())
            if mo != mn:
                gone, added = list((mo - mn).elements())[:3], list((mn - mo).elements())[:3]
                problems.append(f"{path}: inline code of section {k} differs: missing {gone}, added {added}")
    text = "\n".join(p_new)
    for a in sorted(allow, key=len, reverse=True):
        text = text.replace(a, " ")
    for n, line in enumerate(text.split("\n"), 1):
        if JAPANESE.search(line):
            problems.append(f"{path}: Japanese in prose: {line.strip()[:80]}")
    return len(problems) - before


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--allow", default=os.path.join(HERE, "translation-allow.txt"))
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    problems = []
    allow = read_allow(a.allow, problems)
    rules = label_rules(read_labels())
    texts = []
    for f in a.files:
        check_file(f, a.base, rules, allow, problems)
        texts.append(open(f, encoding="utf-8").read())
    for entry in allow:
        if not any(entry in t for t in texts):
            problems.append(f"{a.allow}: {entry!r} occurs in no file")
    for p in problems:
        print(p, file=sys.stderr)
    print(f"translation: {len(a.files)} files, {len(problems)} problems")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
