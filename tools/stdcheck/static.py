# SPDX-License-Identifier: MIT OR Apache-2.0
import sys

from common import LOWER, UNIT_NAME, UPPER, Defs, parts, quantity, source_of, walk_values

FCT_FIELDS = {"refuse", "weightChange"}


def naming(defs):
    bad = []
    upper = {"dimension", "trait", "ingredient", "alias", "kind", "vessel", "enum"}
    lower = {"action", "axis", "dirt", "rule", "property", "source", "resource"}
    for d in defs.all:
        if not d["module"].startswith("std."):
            continue
        name, kind = d["name"], d["kind"]
        if kind in upper and not UPPER.match(name):
            bad.append(name)
        elif kind in lower and not LOWER.match(name):
            bad.append(name)
        elif kind == "unit" and not UNIT_NAME.match(name):
            bad.append(name)
        bad += [v for v in d["values"] if not LOWER.match(v)]
        bad += [p for p in parts(d) if not LOWER.match(p)]
        bad += [p["name"] for p in d["params"] if not LOWER.match(p["name"])]
        for m in d["members"]:
            if m.get("field") == "modes":
                bad += [v["text"] for v in m["values"] if not LOWER.match(v["text"])]
    return bad


def provisional_points(defs):
    bad = []
    for d in defs.of_kind("action", True) + defs.of_kind("ingredient", True) + defs.of_kind("kind", True):
        for path, value in walk_values(d):
            if source_of(value) == "provisional":
                q = quantity(value["text"])
                if q is None or q[0] >= q[1]:
                    bad.append(".".join(path))
    return bad


def fct_inline(defs):
    bad = []
    for d in defs.all:
        if not d["module"].startswith("std."):
            continue
        for path, value in walk_values(d):
            if path[-1] in FCT_FIELDS and not value["text"].startswith("fct("):
                bad.append(".".join(path))
            elif source_of(value) == "mextFct2023":
                bad.append(".".join(path))
    return bad


def unprovided(defs):
    provided = set()
    kinds = {k["name"]: k for k in defs.of_kind("kind")}

    def closure(name, seen):
        if name in seen or name not in kinds:
            return set()
        seen.add(name)
        out = set()
        for s in kinds[name]["supers"]:
            out.add(s.get("name"))
            out |= closure(s.get("name"), seen)
        return out
    for name in kinds:
        provided |= closure(name, set())
    if defs.of_kind("vessel"):
        provided.add("Vessel")
    return [c["name"] for c in defs.capabilities() if c["module"].startswith("std.") and c["name"] not in provided]


def main(argv):
    if len(argv) != 2:
        print(f"usage: {argv[0]} DEFINITIONS", file=sys.stderr)
        return 2
    defs = Defs(argv[1])
    checks = [("naming", naming(defs)), ("provisional-point", provisional_points(defs)),
              ("fct-inline", fct_inline(defs)), ("capability-unprovided", unprovided(defs))]
    for label, items in checks:
        for item in items:
            print(f"{label}: {item}", file=sys.stderr)
    n = sum(1 for d in defs.all if d["module"].startswith("std."))
    print(f"std-static: {n} definitions, " + ", ".join(f"{len(items)} {label}" for label, items in checks))
    return 1 if any(items for _, items in checks) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
