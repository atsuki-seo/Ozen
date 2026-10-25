#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import argparse
import json
import os
import re

import jsonschema
import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import qty as qtymod

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FILES = ["dimensions", "units", "axes", "traits", "ingredients", "aliases", "actions",
         "capabilities", "dirt", "vessels", "sources"]
UPPER = re.compile(r"^[A-Z][A-Za-z0-9]*$")
LOWER = re.compile(r"^[a-z][A-Za-z0-9]*$")
UNIT_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)*$")
RULE_KEYWORDS = {"allergicPerson", "dessert", "sameFoodSameState", "sameTool"}
BUILTIN_TYPES = {"AllergenKind"}
PROP_DIMENSIONS = {
    "density": ("Mass", "Volume"), "unitWeight": ("Mass", "Count"),
    "heatUp": ("Time",), "soften": ("Time",), "direct": ("Time",), "time": ("Time",),
    "washTime": ("Time",), "wipeTime": ("Time",),
}
SEVERITIES = ("error", "warning", "info")


def fmt_path(parts):
    out = ""
    for p in parts:
        if isinstance(p, int):
            out += f"[{p}]"
        else:
            out += ("." if out else "") + str(p)
    return out


class Diags:
    def __init__(self):
        self.items = []

    def add(self, name, file, path, args=(), severity="error"):
        self.items.append({"severity": severity, "name": name, "file": file,
                           "path": fmt_path(path) if isinstance(path, (list, tuple)) else path,
                           "args": [str(a) for a in args]})

    def count(self, severity):
        return sum(1 for d in self.items if d["severity"] == severity)


class DedupLoader(yaml.SafeLoader):
    pass


def _dedupe(node, path, fname, diags):
    if isinstance(node, yaml.MappingNode):
        seen, kept = set(), []
        for k, v in node.value:
            key = k.value if isinstance(k, yaml.ScalarNode) else None
            if key is not None and key in seen:
                diags.add("NAM:duplicate", fname, path + [key], [key])
                continue
            if key is not None:
                seen.add(key)
            kept.append((k, v))
            _dedupe(v, path + ([key] if key is not None else []), fname, diags)
        node.value = kept
    elif isinstance(node, yaml.SequenceNode):
        for i, v in enumerate(node.value):
            _dedupe(v, path + [i], fname, diags)


def load_yaml(path, fname, diags):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    loader = DedupLoader(text)
    try:
        node = loader.get_single_node()
        if node is None:
            return {}
        _dedupe(node, [], fname, diags)
        return loader.construct_document(node)
    except yaml.YAMLError as e:
        diags.add("FMT:yaml", fname, "", [getattr(e, "problem", None) or str(e)])
        return None
    finally:
        loader.dispose()


def load_vocab(vocab_dir, diags):
    data = {}
    for stem in FILES:
        p = os.path.join(vocab_dir, stem + ".yaml")
        if os.path.exists(p):
            data[stem] = load_yaml(p, stem + ".yaml", diags)
    return data


def schema_validator(stem):
    with open(os.path.join(ROOT, "spec", "schema", "vocab.schema.json"), encoding="utf-8") as f:
        root = json.load(f)
    resolver = jsonschema.RefResolver.from_schema(root)
    return jsonschema.Draft202012Validator(root["$defs"]["files"][stem], resolver=resolver)


def schema_errors(stem, doc, fname, diags):
    blocking = 0
    validator = schema_validator(stem)
    for err in sorted(validator.iter_errors(doc), key=lambda e: [str(p) for p in e.absolute_path]):
        path = list(err.absolute_path)
        schema = err.schema if isinstance(err.schema, dict) else {}
        if err.validator == "additionalProperties" and isinstance(err.instance, dict):
            allowed = set(schema.get("properties", {}))
            for key in err.instance:
                if key not in allowed:
                    diags.add("FMT:schema", fname, path + [key], ["additionalProperties"])
                    blocking += 1
        elif err.validator == "not" and "x-diag" in schema:
            diags.add(schema["x-diag"], fname, path, [])
        elif err.validator == "required" and "x-diag-required" in schema:
            missing = re.match(r"'([^']*)' is a required property", err.message)
            name = schema["x-diag-required"].get(missing.group(1)) if missing else None
            if name:
                diags.add(name, fname, path, [path[-1] if path else ""])
            else:
                diags.add("FMT:schema", fname, path, ["required"])
                blocking += 1
        else:
            diags.add("FMT:schema", fname, path, [err.validator])
            blocking += 1
    return blocking


def run_schema(vocab_dir, diags):
    data = load_vocab(vocab_dir, diags)
    blocking = 0
    names = sorted(n for n in os.listdir(vocab_dir) if n.endswith(".yaml")) if os.path.isdir(vocab_dir) else []
    for n in names:
        if n[:-5] not in FILES:
            diags.add("FMT:unknownFile", n, "", [n])
            blocking += 1
    for stem, doc in data.items():
        if doc is None:
            blocking += 1
            continue
        blocking += schema_errors(stem, doc, stem + ".yaml", diags)
    return data, names, blocking


class Checker:
    def __init__(self, data, diags):
        self.d = {k: (v if isinstance(v, dict) else {}) for k, v in data.items()}
        self.diags = diags
        self.ns = {}

    def get(self, stem, key):
        v = self.d.get(stem, {}).get(key)
        return v if isinstance(v, dict) else ({} if not isinstance(v, list) else v)

    def err(self, name, stem, path, args=()):
        self.diags.add(name, stem + ".yaml", path, args)


    def build(self):
        self.dimensions = self.get("dimensions", "dimensions")
        self.units = self.get("units", "units")
        self.axes = self.get("axes", "axes")
        self.enums = self.get("axes", "enums")
        self.traits = self.get("traits", "traits")
        self.allergens = self.d.get("traits", {}).get("allergens") or []
        self.ingredients = self.get("ingredients", "ingredients")
        self.aliases = self.get("aliases", "aliases")
        self.actions = self.get("actions", "actions")
        self.capabilities = self.get("capabilities", "capabilities")
        self.kinds = self.get("capabilities", "kinds")
        self.dirt = self.get("dirt", "dirt")
        self.rules = self.get("dirt", "rules")
        self.vessels = self.get("vessels", "vessels")
        self.fallbacks = self.get("vessels", "fallbacks")
        self.sources = self.get("sources", "sources")

    def define(self, ns, name, stem, path, report=True):
        table = self.ns.setdefault(ns, {})
        if name in table:
            if report:
                self.err("NAM:duplicate", stem, path, [name])
        else:
            table[name] = (stem, path)

    def definitions(self):
        groups = [
            ("types", "dimensions", "dimensions", self.dimensions),
            ("units", "units", "units", self.units),
            ("axes", "axes", "axes", self.axes),
            ("types", "axes", "enums", self.enums),
            ("types", "traits", "traits", self.traits),
            ("types", "ingredients", "ingredients", self.ingredients),
            ("types", "aliases", "aliases", self.aliases),
            ("actions", "actions", "actions", self.actions),
            ("types", "capabilities", "capabilities", self.capabilities),
            ("types", "capabilities", "kinds", self.kinds),
            ("dirt", "dirt", "dirt", self.dirt),
            ("rules", "dirt", "rules", self.rules),
            ("types", "vessels", "vessels", self.vessels),
            ("fallbacks", "vessels", "fallbacks", self.fallbacks),
            ("sources", "sources", "sources", self.sources),
        ]
        for ns, stem, key, table in groups:
            for name in table:
                self.define(ns, name, stem, [key, name])
                if ns == "axes":
                    for i, v in enumerate((table[name] or {}).get("values") or []):
                        self.define("values." + str(name), v, stem, [key, name, "values", i], report=False)

    def is_type(self, name):
        return name in self.ns.get("types", {}) or name in BUILTIN_TYPES

    def parts_of(self, ingredient):
        ing = self.ingredients.get(ingredient)
        if not isinstance(ing, dict):
            return {}
        parts = dict(ing.get("parts") or {})
        for t in self.trait_closure(ing.get("traits") or []):
            parts.update((self.traits.get(t) or {}).get("parts") or {})
        return parts

    def trait_closure(self, names):
        out, todo = [], [self.trait_base(n) for n in names]
        while todo:
            t = todo.pop(0)
            if t in out or t not in self.traits:
                continue
            out.append(t)
            todo.extend((self.traits.get(t) or {}).get("extends") or [])
        return out

    @staticmethod
    def trait_base(name):
        m = re.match(r"^([A-Za-z0-9]+)\((.*)\)$", str(name))
        return m.group(1) if m else str(name)


    def ref(self, kind, name, stem, path):
        ok = {
            "dimension": lambda n: n in self.dimensions,
            "type": self.is_type,
            "ingredient": lambda n: n in self.ingredients or n in self.aliases,
            "kind": lambda n: n in self.kinds,
            "vessel": lambda n: n in self.vessels,
            "dirt": lambda n: n in self.dirt,
            "axis": lambda n: n in self.axes,
            "action": lambda n: n in self.actions,
            "enum": lambda n: n in self.enums,
        }[kind](name)
        if not ok:
            self.err("NAM:undefined", stem, path, [name])
        return ok

    def ref_trait(self, name, stem, path):
        m = re.match(r"^([A-Za-z0-9]+)\((.*)\)$", str(name))
        base = m.group(1) if m else str(name)
        if base not in self.traits:
            self.err("NAM:undefined", stem, path, [base])
            return False
        if m and m.group(2) not in self.allergens:
            self.err("NAM:undefined", stem, path, [m.group(2)])
            return False
        return True

    def cap_or_kind(self, text, stem, path, allow_hands=True):
        if allow_hands and text == "hands":
            return
        m = re.match(r"^([A-Za-z0-9]+)(?:<([^>]*)>)?$", str(text))
        if not m:
            self.err("NAM:undefined", stem, path, [text])
            return
        base, arg = m.groups()
        if base in self.capabilities:
            params = (self.capabilities.get(base) or {}).get("params") or []
            if arg is not None and arg not in params:
                self.err("NAM:undefined", stem, path, [arg])
        elif base in self.kinds and arg is None:
            pass
        else:
            self.err("NAM:undefined", stem, path, [base])

    def quantity(self, text, stem, path, dims=None):
        q = qtymod.parse(text, set(self.units))
        if not q:
            m = re.match(r"^unknown unit (.*)$", q.reason)
            if m:
                self.err("NAM:undefined", stem, path, [m.group(1)])
            else:
                self.err("TYP:quantity", stem, path, [text])
            return None
        if dims is not None:
            got = tuple(self.unit_dimension(u) for u in q.units)
            if None not in got and got != tuple(dims):
                self.err("TYP:dimension", stem, path, ["/".join(dims), "/".join(got)])
        return q

    def unit_dimension(self, name):
        u = self.units.get(name)
        return u.get("dimension") if isinstance(u, dict) else None

    def prop(self, value, stem, path, key):
        if not isinstance(value, dict):
            return
        src = value.get("source")
        if src is None:
            return
        if src not in self.sources:
            self.err("NAM:provenance", stem, path + ["source"], [src])
            return
        if "value" in value and isinstance(value["value"], str) and src != "mext-fct-2023":
            q = self.quantity(value["value"], stem, path + ["value"], PROP_DIMENSIONS.get(key))
            kind = (self.sources.get(src) or {}).get("kind")
            if q and kind == "provisional" and q.is_point:
                self.err("TYP:provisionalPoint", stem, path + ["value"], [value["value"]])

    def food_props(self, props, stem, path):
        if not isinstance(props, dict):
            return
        for key, value in props.items():
            if key in ("hardness", "fat"):
                enum = {"hardness": "Hardness", "fat": "FatLevel"}[key]
                if value not in ((self.enums.get(enum) or {}).get("values") or []):
                    self.err("NAM:undefined", stem, path + [key], [value])
            elif key == "heating":
                for method, group in (value or {}).items():
                    for k, v in (group or {}).items():
                        p = path + [key, method, k]
                        if k == "at":
                            self.quantity(v, stem, p, ("Length",))
                        else:
                            self.prop(v, stem, p, k)
            else:
                self.prop(value, stem, path + [key], key)

    def state(self, state, axes_used, stem, path):
        for axis, value in (state or {}).items():
            if not self.ref("axis", axis, stem, path + [axis]):
                continue
            values = (self.axes.get(axis) or {}).get("values") or []
            items = value if isinstance(value, list) else [value]
            for i, item in enumerate(items):
                name = next(iter(item)) if isinstance(item, dict) else item
                p = path + [axis] + ([i] if isinstance(value, list) else [])
                if name not in values:
                    self.err("NAM:undefined", stem, p, [name])


    def check_units(self):
        for name, u in self.units.items():
            if not isinstance(u, dict):
                continue
            self.ref("dimension", u.get("dimension"), "units", ["units", name, "dimension"])
            if "def" in u:
                self.quantity(u["def"], "units", ["units", name, "def"])
        reported = set()
        for name, u in self.units.items():
            if name in reported or not isinstance(u, dict) or "def" not in u:
                continue
            chain, cur = [], name
            while True:
                q = qtymod.parse((self.units.get(cur) or {}).get("def"), set(self.units))
                if not q or "/" in q.unit:
                    break
                chain.append(cur)
                cur = q.unit
                if cur == name:
                    if not reported.intersection(chain):
                        self.err("TYP:unitCycle", "units", ["units", name, "def"], chain)
                        reported.update(chain)
                    break
                if cur in chain or "def" not in (self.units.get(cur) or {}):
                    break
        for name, u in self.units.items():
            if not isinstance(u, dict) or "def" not in u:
                continue
            q = qtymod.parse(u["def"], set(self.units))
            if q and "/" not in q.unit:
                target = self.unit_dimension(q.unit)
                if target is not None and u.get("dimension") in self.dimensions and target != u.get("dimension"):
                    self.err("TYP:unitDimension", "units", ["units", name, "def"], [u.get("dimension"), target])
        for dim in self.dimensions:
            n = sum(1 for u in self.units.values() if isinstance(u, dict) and u.get("dimension") == dim and u.get("base"))
            if n != 1:
                self.err("TYP:baseUnit", "dimensions", ["dimensions", dim], [dim, n])

    def check_axes(self):
        for name, ax in self.axes.items():
            values = (ax or {}).get("values") or []
            seen = set()
            for i, v in enumerate(values):
                if v in seen:
                    self.err("TYP:axisDuplicate", "axes", ["axes", name, "values", i], [v])
                seen.add(v)
            for value, params in ((ax or {}).get("params") or {}).items():
                if value not in values:
                    self.err("NAM:undefined", "axes", ["axes", name, "params", value], [value])
                for pname, ptype in (params or {}).items():
                    self.ref("type", ptype, "axes", ["axes", name, "params", value, pname])
        for name, en in self.enums.items():
            values = (en or {}).get("values") or []
            finer = (en or {}).get("finer") or {}
            for key, targets in finer.items():
                if key not in values:
                    self.err("NAM:undefined", "axes", ["enums", name, "finer", key], [key])
                for i, t in enumerate(targets or []):
                    if t not in values:
                        self.err("NAM:undefined", "axes", ["enums", name, "finer", key, i], [t])
            reported = set()
            for key in finer:
                if key in reported:
                    continue
                cycle = self.find_cycle(key, finer)
                if cycle and not reported.intersection(cycle):
                    self.err("TYP:cutCycle", "axes", ["enums", name, "finer", key], cycle)
                    reported.update(cycle)

    @staticmethod
    def find_cycle(start, graph):
        stack = [(start, [start])]
        seen = set()
        while stack:
            node, path = stack.pop(0)
            for nxt in graph.get(node) or []:
                if nxt == start:
                    return path
                if nxt not in seen:
                    seen.add(nxt)
                    stack.append((nxt, path + [nxt]))
        return None

    def check_parts(self, parts, stem, path):
        for pname, part in (parts or {}).items():
            p = path + [pname]
            if pname in self.traits:
                self.err("NAM:partTraitClash", stem, p, [pname])
            part = part or {}
            if "extends" in part and part["extends"] not in parts:
                self.err("NAM:undefined", stem, p + ["extends"], [part["extends"]])
            for i, t in enumerate(part.get("traits") or []):
                self.ref_trait(t, stem, p + ["traits", i])
            self.food_props(part.get("props"), stem, p + ["props"])

    def check_traits(self):
        for name, t in self.traits.items():
            t = t or {}
            for i, e in enumerate(t.get("extends") or []):
                self.ref_trait(e, "traits", ["traits", name, "extends", i])
            for pname, ptype in (t.get("params") or {}).items():
                self.ref("type", ptype, "traits", ["traits", name, "params", pname])
            self.check_parts(t.get("parts"), "traits", ["traits", name, "parts"])

    def check_ingredients(self):
        for name, ing in self.ingredients.items():
            ing = ing or {}
            p = ["ingredients", name]
            for i, t in enumerate(ing.get("traits") or []):
                self.ref_trait(t, "ingredients", p + ["traits", i])
            for i, a in enumerate(ing.get("axes") or []):
                self.ref("axis", a, "ingredients", p + ["axes", i])
            self.state(ing.get("initial"), ing.get("axes") or [], "ingredients", p + ["initial"])
            for i, dname in enumerate(ing.get("leaves") or []):
                self.ref("dirt", dname, "ingredients", p + ["leaves", i])
            self.food_props(ing.get("props"), "ingredients", p + ["props"])
            self.check_parts(ing.get("parts"), "ingredients", p + ["parts"])

    def check_aliases(self):
        for name, al in self.aliases.items():
            al = al or {}
            p = ["aliases", name]
            if not self.ref("ingredient", al.get("of"), "aliases", p + ["of"]):
                continue
            parts = self.parts_of(al.get("of"))
            for i, part in enumerate(al.get("parts") or []):
                if part not in parts:
                    self.err("NAM:undefined", "aliases", p + ["parts", i], [part])
            for i, t in enumerate(al.get("partTraits") or []):
                self.ref_trait(t, "aliases", p + ["partTraits", i])
            self.state(al.get("state"), [], "aliases", p + ["state"])

    def axis_value(self, axis, value, stem, path):
        values = (self.axes.get(axis) or {}).get("values") or []
        if value not in values:
            self.err("TYP:axisValue", stem, path, [axis, value])

    def check_effects(self, effects, params, stem, path):
        for axis, eff in (effects or {}).items():
            p = path + [axis]
            if not self.ref("axis", axis, stem, p):
                continue
            if isinstance(eff, str):
                self.axis_value(axis, eff, stem, p)
                continue
            if not isinstance(eff, dict):
                continue
            op = "add" if "add" in eff else "set"
            self.axis_value(axis, eff.get(op), stem, p + [op])
            for k, v in (eff.get("args") or {}).items():
                if isinstance(v, str) and v.startswith("$") and v[1:] not in params:
                    self.err("NAM:undefined", stem, p + ["args", k], [v[1:]])

    def check_usage(self, u, params, stem, path):
        for i, x in enumerate(u.get("uses") or []):
            self.cap_or_kind(x, stem, path + ["uses", i])
        if "time" in u:
            self.prop(u["time"], stem, path + ["time"], "time")
        for i, lv in enumerate(u.get("leaves") or []):
            lp = path + ["leaves", i]
            if "dirt" in lv:
                self.ref("dirt", lv["dirt"], stem, lp + ["dirt"])
            if "from" in lv and lv["from"] not in params:
                self.err("NAM:undefined", stem, lp + ["from"], [lv["from"]])
            for j, on in enumerate(lv.get("on") or []):
                self.cap_or_kind(on, stem, lp + ["on", j], allow_hands=False)

    def check_actions(self):
        for name, act in self.actions.items():
            act = act or {}
            p = ["actions", name]
            params = [x.get("name") for x in act.get("params") or [] if isinstance(x, dict)]
            for i, prm in enumerate(act.get("params") or []):
                pp = p + ["params", i]
                kind = prm.get("kind")
                if "dimension" in prm:
                    self.ref("dimension", prm["dimension"], "actions", pp + ["dimension"])
                if "enum" in prm and self.ref("enum", prm["enum"], "actions", pp + ["enum"]):
                    if "default" in prm and prm["default"] not in (self.enums[prm["enum"]] or {}).get("values", []):
                        self.err("NAM:undefined", "actions", pp + ["default"], [prm["default"]])
                if kind == "quantity" and "default" in prm:
                    self.quantity(prm["default"], "actions", pp + ["default"],
                                  (prm.get("dimension"),) if prm.get("dimension") in self.dimensions else None)
                for j, alt in enumerate(prm.get("requires") or []):
                    ap = pp + ["requires", j]
                    if "type" in alt:
                        self.ref("type", alt["type"], "actions", ap + ["type"])
                    for k, t in enumerate(alt.get("traits") or []):
                        self.ref_trait(t, "actions", ap + ["traits", k])
                    for axis, cond in (alt.get("axes") or {}).items():
                        cp = ap + ["axes", axis]
                        if not self.ref("axis", axis, "actions", cp):
                            continue
                        for op, val in (cond or {}).items():
                            if isinstance(val, list):
                                for k, v in enumerate(val):
                                    self.axis_value(axis, v, "actions", cp + [op, k])
                            else:
                                self.axis_value(axis, val, "actions", cp + [op])
                self.check_effects(prm.get("effects"), params, "actions", pp + ["effects"])
            out = act.get("output")
            if isinstance(out, dict):
                if "param" in out and out["param"] not in params:
                    self.err("NAM:undefined", "actions", p + ["output", "param"], [out["param"]])
                if "type" in out:
                    self.ref("type", out["type"], "actions", p + ["output", "type"])
                self.check_effects(out.get("effects"), params, "actions", p + ["output", "effects"])
            self.check_usage(act, params, "actions", p)
            for i, var in enumerate(act.get("variants") or []):
                vp = p + ["variants", i]
                self.check_usage(var, params, "actions", vp)
                for j, step in enumerate(var.get("steps") or []):
                    self.ref("action", step, "actions", vp + ["steps", j])

    def kind_provides(self, name, seen=None):
        seen = seen or set()
        if name in seen or name not in self.kinds:
            return set()
        seen.add(name)
        k = self.kinds.get(name) or {}
        out = {re.sub(r"<.*>$", "", str(x)) for x in k.get("provides") or []}
        for parent in k.get("extends") or []:
            out |= self.kind_provides(parent, seen)
        return out

    def check_capabilities(self):
        for name, k in self.kinds.items():
            k = k or {}
            p = ["kinds", name]
            for i, e in enumerate(k.get("extends") or []):
                self.ref("kind", e, "capabilities", p + ["extends", i])
            for i, c in enumerate(k.get("provides") or []):
                base = re.sub(r"<.*>$", "", str(c))
                if base not in self.capabilities:
                    self.err("NAM:undefined", "capabilities", p + ["provides", i], [base])
                else:
                    self.cap_or_kind(c, "capabilities", p + ["provides", i], allow_hands=False)
            for key, v in (k.get("props") or {}).items():
                self.prop(v, "capabilities", p + ["props", key], key)
        provided = set()
        for name in self.kinds:
            provided |= self.kind_provides(name)
        if self.vessels:
            provided.add("Vessel")
        for name in self.capabilities:
            if name not in provided:
                self.err("NAM:capabilityUnprovided", "capabilities", ["capabilities", name], [name])

    def check_dirt(self):
        for name, dt in self.dirt.items():
            for i, t in enumerate((dt or {}).get("traits") or []):
                self.ref_trait(t, "dirt", ["dirt", name, "traits", i])
        levels = (self.axes.get("cleaning") or {}).get("values") or []
        for name, r in self.rules.items():
            r = r or {}
            p = ["rules", name]
            self.ref_trait(r.get("after"), "dirt", p + ["after"])
            if r.get("level") not in levels:
                self.err("NAM:undefined", "dirt", p + ["level"], [r.get("level")])
            for i, c in enumerate(r.get("cases") or []):
                when = c.get("when")
                if when and when[0].isupper():
                    self.ref_trait(when, "dirt", p + ["cases", i, "when"])
                elif when not in RULE_KEYWORDS:
                    self.err("NAM:undefined", "dirt", p + ["cases", i, "when"], [when])
                if c.get("level") not in levels:
                    self.err("NAM:undefined", "dirt", p + ["cases", i, "level"], [c.get("level")])
            if self.trait_base(r.get("after")) in ("Hazard", "Allergen") and r.get("fixed") is not True:
                self.err("SAF:ruleNotFixed", "dirt", p, [self.trait_base(r.get("after"))])

    def check_vessels(self):
        for name, v in self.vessels.items():
            v = v or {}
            if "extends" in v:
                self.ref("vessel", v["extends"], "vessels", ["vessels", name, "extends"])
            for i, t in enumerate(v.get("traits") or []):
                self.ref_trait(t, "vessels", ["vessels", name, "traits", i])
        for name, fb in self.fallbacks.items():
            for i, v in enumerate((fb or {}).get("order") or []):
                self.ref("vessel", v, "vessels", ["fallbacks", name, "order", i])

    def check_naming(self):
        upper = [("dimensions", "dimensions", self.dimensions), ("axes", "enums", self.enums),
                 ("traits", "traits", self.traits), ("ingredients", "ingredients", self.ingredients),
                 ("aliases", "aliases", self.aliases), ("capabilities", "capabilities", self.capabilities),
                 ("capabilities", "kinds", self.kinds), ("vessels", "vessels", self.vessels)]
        lower = [("actions", "actions", self.actions), ("axes", "axes", self.axes),
                 ("dirt", "dirt", self.dirt), ("dirt", "rules", self.rules),
                 ("vessels", "fallbacks", self.fallbacks)]
        for stem, key, table in upper:
            for name in table:
                if not UPPER.match(str(name)):
                    self.err("NAM:naming", stem, [key, name], [name])
        for stem, key, table in lower:
            for name in table:
                if not LOWER.match(str(name)):
                    self.err("NAM:naming", stem, [key, name], [name])
        for name in self.units:
            if not UNIT_NAME.match(str(name)):
                self.err("NAM:naming", "units", ["units", name], [name])
        for name, ax in self.axes.items():
            for i, v in enumerate((ax or {}).get("values") or []):
                if not LOWER.match(str(v)):
                    self.err("NAM:naming", "axes", ["axes", name, "values", i], [v])
        for name, en in self.enums.items():
            for i, v in enumerate((en or {}).get("values") or []):
                if not LOWER.match(str(v)):
                    self.err("NAM:naming", "axes", ["enums", name, "values", i], [v])
        for i, a in enumerate(self.allergens):
            if not LOWER.match(str(a)):
                self.err("NAM:naming", "traits", ["allergens", i], [a])
        for name, c in self.capabilities.items():
            for i, a in enumerate((c or {}).get("params") or []):
                if not LOWER.match(str(a)):
                    self.err("NAM:naming", "capabilities", ["capabilities", name, "params", i], [a])
        for stem, key, table in [("traits", "traits", self.traits), ("ingredients", "ingredients", self.ingredients)]:
            for name, entry in table.items():
                for pname in ((entry or {}).get("parts") or {}):
                    if not LOWER.match(str(pname)):
                        self.err("NAM:naming", stem, [key, name, "parts", pname], [pname])
        for name, act in self.actions.items():
            for i, prm in enumerate((act or {}).get("params") or []):
                if isinstance(prm, dict) and not LOWER.match(str(prm.get("name"))):
                    self.err("NAM:naming", "actions", ["actions", name, "params", i, "name"], [prm.get("name")])

    def run(self):
        self.build()
        self.definitions()
        self.check_naming()
        self.check_units()
        self.check_axes()
        self.check_traits()
        self.check_ingredients()
        self.check_aliases()
        self.check_actions()
        self.check_capabilities()
        self.check_dirt()
        self.check_vessels()


def run_check(vocab_dir, diags):
    data, _names, blocking = run_schema(vocab_dir, diags)
    if blocking == 0:
        Checker(data, diags).run()
    return data


def emit(diags, summary, as_json):
    if as_json:
        print(json.dumps({"diagnostics": diags.items, "summary": summary}, ensure_ascii=False, indent=1))
        return
    for d in diags.items:
        print(" ".join([d["severity"], d["name"], d["file"], d["path"] or "-"] + d["args"]), file=sys.stderr)
    print(summary)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="vocabcheck.py")
    ap.add_argument("command", choices=["schema", "check"])
    ap.add_argument("--vocab", default=os.path.join(ROOT, "spec", "vocab"))
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    diags = Diags()
    if args.command == "schema":
        _data, names, _ = run_schema(args.vocab, diags)
        errors = diags.count("error")
        emit(diags, f"schema: {len(names)} files, {errors} errors", args.json)
    else:
        run_check(args.vocab, diags)
        errors = diags.count("error")
        emit(diags, f"check: {errors} errors, {diags.count('warning')} warnings, {diags.count('info')} infos", args.json)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
