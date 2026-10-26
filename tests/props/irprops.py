# SPDX-License-Identifier: MIT OR Apache-2.0
import os
import re
import subprocess

FLAG_WITH_VALUE = {"--kitchen", "--options", "--entry"}


def root_dir():
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def flags_of(first):
    """Return the flags of a golden's args line that matter for the IR, or None to skip the golden."""
    if not first.startswith("// args:"):
        return []
    words = first.split()[2:]
    if not words or words[0] not in ("check", "debug"):
        return None
    if words[0] == "debug" and (len(words) < 2 or words[1] not in ("steps", "ir", "requires")):
        return None
    rest = words[1:] if words[0] == "check" else words[2:]
    out, k = [], 0
    while k < len(rest):
        if rest[k] in FLAG_WITH_VALUE and k + 1 < len(rest):
            out += rest[k:k + 2]
            k += 2
        elif rest[k] == "--strict":
            k += 1
        else:
            return None
    return out


def corpus():
    root = root_dir()
    for d, _, files in sorted(os.walk(os.path.join(root, "tests", "golden"))):
        for f in sorted(files):
            path = os.path.join(d, f)
            if not f.endswith(".ozen") or not os.path.exists(path[:-5] + ".out"):
                continue
            try:
                text = open(path, encoding="utf-8").read()
            except UnicodeDecodeError:
                continue
            lines = text.splitlines()
            flags = flags_of(lines[0] if lines else "")
            if flags is None or not re.search(r"^(recipe|menu) ", text, re.M):
                continue
            body = "\n".join(lines[1:]) + "\n" if lines and lines[0].startswith("// args:") else text
            yield os.path.relpath(path, root), body, flags


def ir_lines(ozen, directory, name, flags):
    target = [name] if name is not None else []
    out = subprocess.run([ozen, "debug", "ir", *flags, *target], cwd=directory, capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if l.startswith("ir ") or l.startswith("  ")]


def write(directory, name, text):
    with open(os.path.join(directory, name), "w", encoding="utf-8") as f:
        f.write(text)
