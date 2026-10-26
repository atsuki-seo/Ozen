# SPDX-License-Identifier: MIT OR Apache-2.0
import json
import re
from fractions import Fraction

UPPER = re.compile(r"^[A-Z][A-Za-z0-9]*$")
LOWER = re.compile(r"^[a-z][A-Za-z0-9]*$")
UNIT_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
QTY = re.compile(r"^\s*([0-9]+(?:\.[0-9]+)?(?:/[0-9]+)?)(?:\.\.([0-9]+(?:\.[0-9]+)?(?:/[0-9]+)?))?\s*(\S+)?\s*$")


class Defs:
    def __init__(self, path):
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        self.all = data["definitions"]
        self.diagnostics = data.get("diagnostics", [])
        self.by = {}
        for d in self.all:
            self.by.setdefault((d["kind"], d["name"]), []).append(d)

    def get(self, kind, name, module=None):
        for d in self.by.get((kind, name), []):
            if module is None or d["module"] == module:
                return d
        return None

    def of_kind(self, kind, std_only=False):
        return [d for d in self.all if d["kind"] == kind and (not std_only or d["module"].startswith("std."))]

    def capabilities(self):
        return [d for d in self.of_kind("trait") if any(s.get("name") == "Capability" for s in d["supers"])]


def fields(members, name):
    return [m for m in members if m.get("field") == name]


def section(members, name):
    for m in members:
        if m.get("section") == name:
            return m["members"]
    return None


def parts(defn):
    found = section(defn["members"], "parts") or []
    names = []
    for m in found:
        names.append(m.get("marker") or m.get("section") or m.get("field"))
    return [n for n in names if n]


def super_names(defn):
    return [s.get("name") for s in defn["supers"] if s.get("name")]


def norm(text):
    return re.sub(r"\s+", " ", text.strip())


def quantity(text):
    m = QTY.match(text or "")
    if not m:
        return None
    lo = Fraction(m.group(1))
    hi = Fraction(m.group(2)) if m.group(2) else lo
    return lo, hi, m.group(3)


def source_of(value):
    for a in value.get("annotations", []):
        if a["name"] == "source" and a["args"]:
            return a["args"][0]
    return None


def walk_values(defn):
    """Yield (path, value) for every field value of a definition, with vocabulary-style paths."""
    stem = {"action": "actions", "ingredient": "ingredients", "kind": "kinds", "trait": "traits"}.get(defn["kind"])
    base = [stem, defn["name"]] if stem else [defn["kind"], defn["name"]]

    def members(ms, path):
        for m in ms:
            if "field" in m:
                for v in m["values"]:
                    merged = dict(v)
                    merged["annotations"] = v.get("annotations", []) + m.get("annotations", [])
                    yield path + [m["field"]], merged
            elif "section" in m:
                yield from members(m["members"], path + [m["section"]])
            elif "uses" in m and m.get("for") is not None:
                v = dict(m["for"])
                v["annotations"] = v.get("annotations", []) + m.get("annotations", [])
                yield path + ["time"], v

    variants = [m for m in defn["members"] if "variant" in m]
    yield from members([m for m in defn["members"] if "variant" not in m], base)
    for i, v in enumerate(variants):
        yield from members(v["members"], base + [f"variants[{i}]"])


def fmt(path):
    out = ""
    for p in path:
        out += p if p.startswith("[") else ("." if out else "") + p
    return out.replace(".variants[", ".variants[")
