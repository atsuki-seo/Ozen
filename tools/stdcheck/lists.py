# SPDX-License-Identifier: MIT OR Apache-2.0
import json
import sys

import yaml

from common import Defs, norm, parts, quantity, section, super_names


class Lists:
    def __init__(self, defs):
        self.d = defs

    def enum_values(self, name):
        e = self.d.get("enum", name)
        return e["values"] if e else []

    def lookup(self, kind, owner, ident):
        d = self.d
        if kind == "unit" and ident.startswith("std."):
            module, _, name = ident.rpartition(".")
            return d.get("unit", name, module) is not None
        simple = {"dimension", "unit", "axis", "ingredient", "alias", "action", "kind", "vessel", "dirt", "rule"}
        if kind in simple:
            return d.get(kind, ident) is not None
        if kind == "trait":
            return d.get("trait", ident) is not None
        if kind == "capability":
            return any(c["name"] == ident for c in d.capabilities())
        if kind == "fallback":
            p = d.get("property", ident)
            return p is not None and p.get("value", {}).get("text", "").startswith("vesselOrder(")
        if kind == "axisValue":
            a = d.get("axis", owner)
            return a is not None and ident in a["values"]
        if kind in ("cutStyle", "heat"):
            return ident in self.enum_values({"cutStyle": "CutStyle", "heat": "Heat"}[kind])
        if kind == "allergen":
            return ident in self.enum_values("AllergenKind")
        if kind == "part":
            owner_def = d.get("ingredient", owner) or d.get("trait", owner)
            return owner_def is not None and ident in parts(owner_def)
        if kind == "capabilityParam":
            cap = next((c for c in d.capabilities() if c["name"] == owner), None)
            modes = [f for f in (cap or {}).get("members", []) if f.get("field") == "modes"]
            return bool(modes) and ident in [v["text"] for v in modes[0]["values"]]
        return False

    def value_ok(self, kind, ident, want):
        d = self.d
        if kind == "unit":
            module, _, name = ident.rpartition(".") if ident.startswith("std.") else (None, "", ident)
            u = d.get("unit", name, module)
            return u is not None and quantity(u["value"]["text"]) == quantity(want)
        if kind == "axis":
            a = d.get("axis", ident)
            return a is not None and a["values"] == list(want)
        if kind == "fallback":
            p = d.get("property", ident)
            text = p["value"]["text"] if p else ""
            inside = text[len("vesselOrder("):-1] if text.endswith(")") else ""
            order = [x.strip() for x in inside.split(",") if ":" not in x]
            return order == list(want)
        return False

    def kinds_and_caps(self, k):
        kind_names = {x["name"] for x in self.d.of_kind("kind")}
        caps = {c["name"] for c in self.d.capabilities()}
        extends = [s["name"] for s in k["supers"] if s.get("name") in kind_names]
        provides = [norm(s["text"]) for s in k["supers"] if s.get("name") in caps]
        return extends, provides

    def alts(self, typ):
        out = set()
        for alt in typ.get("alts", []):
            items = []
            for a in alt:
                items.append(a["bare"] if "bare" in a else f"{a['axis']} {a['op']} {a['value']}")
            out.add(tuple(sorted(items)))
        return out

    def expected_alts(self, requires):
        out = set()
        for alt in requires:
            choices = [[]]
            for t in alt.get("traits", []):
                choices = [c + [t] for c in choices]
            for axis, cond in (alt.get("axes") or {}).items():
                if "in" in cond:
                    choices = [c + [f"{axis} : {v}"] for c in choices for v in cond["in"]]
                if "has" in cond:
                    choices = [c + [f"{axis} has {cond['has']}"] for c in choices]
            out |= {tuple(sorted(c)) for c in choices if c}
        return out

    def relation_ok(self, r):
        d = self.d
        k = r.get("kind")
        if k == "dirtTraits":
            x = d.get("dirt", r["dirt"])
            return x is not None and set(super_names(x)) == set(r["traits"])
        if k == "vesselTrait":
            vessels = d.of_kind("vessel")
            has = {v["name"] for v in vessels if r["trait"] in super_names(v)}
            names = {v["name"] for v in vessels}
            return has == set(r["has"]) and names == set(r["has"]) | set(r["hasNot"])
        if k == "vesselExtends":
            v = d.get("vessel", r["vessel"])
            vessels = {x["name"] for x in d.of_kind("vessel")}
            return v is not None and [s for s in super_names(v) if s in vessels] == [r["extends"]]
        if k == "kindProvides":
            x = d.get("kind", r["of"])
            if x is None:
                return False
            extends, provides = self.kinds_and_caps(x)
            return set(extends) == set(r["extends"]) and set(provides) == set(r["provides"])
        if k == "ruleLevel":
            x = d.get("rule", r["rule"])
            if x is None or x.get("after") != r["after"] or not x["cases"]:
                return False
            default = [c for c in x["cases"] if c["when"] is None]
            cases = {(c["when"], c["level"]) for c in x["cases"] if c["when"] is not None}
            return len(default) == 1 and default[0]["level"] == r["level"] and \
                cases == {(c["when"], c["level"]) for c in r["cases"]}
        if k == "actionSignature":
            x = d.get("action", r["action"])
            if x is None or not x["params"]:
                return False
            p = x["params"][0]["type"] or {}
            want_type = next((alt["type"] for alt in r["requires"] if "type" in alt), "T")
            variants = [m["variant"] for m in x["members"] if "variant" in m]
            return p.get("name") == want_type and self.alts(p) == self.expected_alts(r["requires"]) and \
                (x.get("output") or {}).get("name") == r["output"] and variants == r["variants"]
        if k == "actionEffect":
            x = d.get("action", r["action"])
            if x is None or not x["params"] or not x.get("output"):
                return False
            p = x["params"][0]["type"] or {}
            req_ok = any(all(f"{a} : {v}" in alt for a, vs in r["requiresIn"].items() for v in vs)
                         for alt in self.alts(p))
            out = {a["axis"]: a["value"] for alt in x["output"].get("alts", []) for a in alt if "axis" in a}
            return req_ok and all(out.get(a) == v for a, v in r["effects"].items())
        if k == "actionNoEffect":
            x = d.get("action", r["action"])
            out = (x or {}).get("output") or {}
            return x is not None and all(a.get("axis") != r["axis"] for alt in out.get("alts", []) for a in alt)
        if k == "finer":
            e = d.get("enum", r["enum"])
            graph = {f["from"]: f["to"] for f in (e or {}).get("finer", [])}
            seen, todo = set(), [r["from"]]
            while todo:
                n = todo.pop()
                for m in graph.get(n, []):
                    if m not in seen:
                        seen.add(m)
                        todo.append(m)
            return (r["to"] in seen) == bool(r["reachable"])
        if k == "ingredientTraits":
            x = d.get("ingredient", r["ingredient"])
            return x is not None and set(r["traits"]) <= set(super_names(x))
        return False


def main(argv):
    if len(argv) != 4:
        print(f"usage: {argv[0]} LISTS RELATIONS DEFINITIONS", file=sys.stderr)
        return 2
    lists = Lists(Defs(argv[3]))
    with open(argv[1], encoding="utf-8") as f:
        entries = (yaml.safe_load(f) or {}).get("entries") or []
    with open(argv[2], encoding="utf-8") as f:
        relations = (yaml.safe_load(f) or {}).get("relations") or []
    missing = values = mismatched = rel_bad = 0
    for e in entries:
        label = "/".join(str(x) for x in (e.get("kind"), e.get("owner"), e.get("id")) if x is not None)
        if not lists.lookup(e.get("kind"), e.get("owner"), str(e.get("id"))):
            missing += 1
            print(f"missing: {label}", file=sys.stderr)
            continue
        if "value" in e:
            values += 1
            if not lists.value_ok(e["kind"], str(e["id"]), e["value"]):
                mismatched += 1
                print(f"value mismatch: {label}", file=sys.stderr)
    for i, r in enumerate(relations):
        if not lists.relation_ok(r):
            rel_bad += 1
            print(f"relation mismatch: [{i}] {json.dumps(r, ensure_ascii=False)}", file=sys.stderr)
    print(f"lists: {len(entries)} listed, {missing} missing; values: {values} checked, {mismatched} mismatched; "
          f"relations: {len(relations)} listed, {rel_bad} mismatched")
    return 1 if missing or mismatched or rel_bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
