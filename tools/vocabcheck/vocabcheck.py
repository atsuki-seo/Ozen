#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import argparse
import json
import os
import re
from decimal import Decimal

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
            # YAML 1.1 resolves plain keys such as `on` and `yes` to booleans; the formats use `on`
            # as a key, so every scalar key is read as a string (values keep the implicit types).
            if key is not None:
                k.tag = "tag:yaml.org,2002:str"
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


class Lists:
    def __init__(self, data):
        c = Checker(data, Diags())
        c.build()
        self.c = c

    def lookup(self, kind, owner, ident):
        c = self.c
        tables = {
            "dimension": c.dimensions, "unit": c.units, "axis": c.axes, "trait": c.traits,
            "ingredient": c.ingredients, "alias": c.aliases, "action": c.actions,
            "capability": c.capabilities, "kind": c.kinds, "vessel": c.vessels, "dirt": c.dirt,
            "rule": c.rules, "fallback": c.fallbacks,
        }
        if kind in tables:
            return ident in tables[kind]
        if kind == "axisValue":
            return ident in ((c.axes.get(owner) or {}).get("values") or [])
        if kind in ("cutStyle", "heat"):
            enum = {"cutStyle": "CutStyle", "heat": "Heat"}[kind]
            return ident in ((c.enums.get(enum) or {}).get("values") or [])
        if kind == "part":
            entry = c.ingredients.get(owner) if owner in c.ingredients else c.traits.get(owner)
            return ident in ((entry or {}).get("parts") or {})
        if kind == "capabilityParam":
            return ident in ((c.capabilities.get(owner) or {}).get("params") or [])
        if kind == "allergen":
            return ident in c.allergens
        return False

    def value_ok(self, kind, ident, want):
        c = self.c
        if kind == "unit":
            got = qtymod.parse((c.units.get(ident) or {}).get("def"), set(c.units))
            exp = qtymod.parse(want, set(c.units))
            return bool(got) and bool(exp) and (got.lo, got.hi, got.unit) == (exp.lo, exp.hi, exp.unit)
        if kind == "axis":
            return list((c.axes.get(ident) or {}).get("values") or []) == list(want)
        if kind == "fallback":
            return list((c.fallbacks.get(ident) or {}).get("order") or []) == list(want)
        return False

    def relation_ok(self, r):
        c = self.c
        k = r.get("kind")
        if k == "dirtTraits":
            return set((c.dirt.get(r["dirt"]) or {}).get("traits") or []) == set(r["traits"])
        if k == "vesselTrait":
            def has(v):
                return r["trait"] in ((c.vessels.get(v) or {}).get("traits") or [])
            return all(has(v) for v in r["has"]) and not any(has(v) for v in r["hasNot"]) \
                and set(r["has"]) | set(r["hasNot"]) == set(c.vessels)
        if k == "vesselExtends":
            return (c.vessels.get(r["vessel"]) or {}).get("extends") == r["extends"]
        if k == "kindProvides":
            kd = c.kinds.get(r["of"]) or {}
            return set(kd.get("extends") or []) == set(r["extends"]) and \
                set(kd.get("provides") or []) == set(r["provides"])
        if k == "ruleLevel":
            rule = c.rules.get(r["rule"]) or {}
            cases = {(x.get("when"), x.get("level")) for x in rule.get("cases") or []}
            return rule.get("after") == r["after"] and rule.get("level") == r["level"] and \
                cases == {(x["when"], x["level"]) for x in r["cases"]}
        if k == "actionSignature":
            act = c.actions.get(r["action"]) or {}
            params = act.get("params") or []
            return bool(params) and params[0].get("requires") == r["requires"] and \
                (act.get("output") or {}).get("type") == r["output"] and \
                [v.get("name") for v in act.get("variants") or []] == r["variants"]
        if k == "actionEffect":
            act = c.actions.get(r["action"]) or {}
            prm = (act.get("params") or [{}])[0]
            ins = [{a: (cond or {}).get("in") for a, cond in (alt.get("axes") or {}).items()}
                   for alt in prm.get("requires") or []]
            return any(all(x.get(a) == v for a, v in r["requiresIn"].items()) for x in ins) and \
                all((prm.get("effects") or {}).get(a) == v for a, v in r["effects"].items())
        if k == "actionNoEffect":
            act = c.actions.get(r["action"]) or {}
            return all(r["axis"] not in (p.get("effects") or {}) for p in act.get("params") or []) and \
                r["axis"] not in (((act.get("output") or {}) if isinstance(act.get("output"), dict) else {}).get("effects") or {})
        if k == "finer":
            graph = (c.enums.get(r["enum"]) or {}).get("finer") or {}
            seen, todo = set(), [r["from"]]
            while todo:
                n = todo.pop()
                for m in graph.get(n) or []:
                    if m not in seen:
                        seen.add(m)
                        todo.append(m)
            return (r["to"] in seen) == bool(r["reachable"])
        if k == "ingredientTraits":
            return set(r["traits"]) <= set((c.ingredients.get(r["ingredient"]) or {}).get("traits") or [])
        return False

    def run(self, lists_path, relations_path):
        with open(lists_path, encoding="utf-8") as f:
            entries = (yaml.safe_load(f) or {}).get("entries") or []
        with open(relations_path, encoding="utf-8") as f:
            relations = (yaml.safe_load(f) or {}).get("relations") or []
        missing = values = mismatched = rel_bad = 0
        for e in entries:
            label = "/".join(str(x) for x in (e.get("kind"), e.get("owner"), e.get("id")) if x is not None)
            if not self.lookup(e.get("kind"), e.get("owner"), str(e.get("id"))):
                missing += 1
                print(f"missing: {label}", file=sys.stderr)
                continue
            if "value" in e:
                values += 1
                if not self.value_ok(e["kind"], str(e["id"]), e["value"]):
                    mismatched += 1
                    print(f"value mismatch: {label}", file=sys.stderr)
        for i, r in enumerate(relations):
            if not self.relation_ok(r):
                rel_bad += 1
                print(f"relation mismatch: [{i}] {json.dumps(r, ensure_ascii=False)}", file=sys.stderr)
        print(f"lists: {len(entries)} listed, {missing} missing; values: {values} checked, {mismatched} mismatched; "
              f"relations: {len(relations)} listed, {rel_bad} mismatched")
        return 1 if missing or mismatched or rel_bad else 0


def fmt_fraction(x):
    if x.denominator == 1:
        return str(x.numerator)
    d = x.denominator
    while d % 2 == 0:
        d //= 2
    while d % 5 == 0:
        d //= 5
    if d == 1:
        text = format(Decimal(x.numerator) / Decimal(x.denominator), "f")
        return text.rstrip("0").rstrip(".") if "." in text else text
    return f"{x.numerator}/{x.denominator}"


def fmt_amount(lo, hi, unit):
    if lo == hi:
        return f"{fmt_fraction(lo)} {unit}"
    return f"{fmt_fraction(lo)}..{fmt_fraction(hi)} {unit}"


class Value:
    def __init__(self, type_, axes, state, traits, amount, part=None, unknown=False):
        self.type = type_
        self.axes = list(axes)
        self.state = state
        self.traits = set(traits)
        self.amount = amount
        self.part = part
        self.unknown = unknown

    def copy(self):
        state = {}
        for k, v in self.state.items():
            if isinstance(v, tuple):
                state[k] = (v[0], dict(v[1]))
            elif isinstance(v, dict):
                state[k] = {i: dict(a) for i, a in v.items()}
            else:
                state[k] = v
        return Value(self.type, self.axes, state, self.traits, self.amount, self.part, self.unknown)


UNKNOWN = Value(None, [], {}, [], None, unknown=True)


class Entry:
    def __init__(self, value, at, key):
        self.value = value
        self.at = at
        self.key = key
        self.consumed = False
        self.returned = False


class Sampler:
    def __init__(self, checker, doc):
        self.c = checker
        self.doc = doc
        self.diags = []
        self.steps = {}
        self.states = {}
        self.requires = set()
        self.units = set(checker.units)

    def diag(self, severity, name, at, key, args):
        self.diags.append({"severity": severity, "name": name, "at": at, "args": [str(a) for a in args], "_key": key})

    def err(self, name, at, key, *args):
        self.diag("error", name, at, key, args)

    def info(self, name, at, key, *args):
        self.diag("info", name, at, key, args)

    def closure(self, traits):
        out = set()
        for t in traits:
            out.add(t)
            base = Checker.trait_base(t)
            out.add(base)
            out.update(self.c.trait_closure([base]))
        return out

    def axis_kind(self, axis):
        return (self.c.axes.get(axis) or {}).get("kind")

    def axis_values(self, axis):
        return (self.c.axes.get(axis) or {}).get("values") or []

    def initial_state(self, axes, initial):
        state = {}
        for a in axes:
            kind = self.axis_kind(a)
            values = self.axis_values(a)
            if kind == "ordered":
                state[a] = values[0] if values else None
            elif kind == "tagged":
                state[a] = (values[0] if values else None, {})
            elif kind == "set":
                state[a] = {}
        for a, v in (initial or {}).items():
            if a not in state:
                continue
            kind = self.axis_kind(a)
            if kind == "ordered":
                state[a] = v
            elif kind == "tagged":
                state[a] = (next(iter(v)), dict(next(iter(v.values())) or {})) if isinstance(v, dict) else (v, {})
            elif kind == "set":
                state[a] = {x: {} for x in (v if isinstance(v, list) else [v])}
        return state

    def new_value(self, type_, amount=None):
        ing = self.c.ingredients.get(type_) or {}
        return Value(type_, ing.get("axes") or [], self.initial_state(ing.get("axes") or [], ing.get("initial")),
                     ing.get("traits") or [], amount)

    def ingredient(self, rname, name, spec, at, key):
        t = spec.get("type")
        amount = None
        if "amount" in spec:
            amount = qtymod.parse(spec["amount"], self.units)
            if not amount:
                self.err("TYP:quantity", at, key, spec["amount"])
                amount = None
        if t in self.c.ingredients:
            v = self.new_value(t, amount)
            part = spec.get("part")
            if part is not None:
                parts = self.c.parts_of(t)
                if part not in parts:
                    self.err("NAM:undefined", at, key, part)
                else:
                    v.part = part
                    v.traits |= set((parts.get(part) or {}).get("traits") or [])
            return v
        if t in self.c.aliases:
            al = self.c.aliases.get(t) or {}
            v = self.new_value(al.get("of"), amount)
            parts = self.c.parts_of(al.get("of"))
            cands = [p for p in parts if p in (al.get("parts") or []) or
                     (al.get("partTraits") and set(al["partTraits"]) <= set((parts.get(p) or {}).get("traits") or []))]
            common = None
            for p in cands:
                pt = set((parts.get(p) or {}).get("traits") or [])
                common = pt if common is None else common & pt
            v.traits |= common or set()
            for a, st in (al.get("state") or {}).items():
                if a in v.state and self.axis_kind(a) == "tagged":
                    v.state[a] = (next(iter(st)), dict(next(iter(st.values())) or {})) if isinstance(st, dict) else (st, {})
            return v
        self.err("NAM:undefined", at, key, t)
        return UNKNOWN

    def to_base(self, lo, hi, unit):
        seen = set()
        while True:
            u = self.c.units.get(unit) or {}
            if u.get("base") or "def" not in u or unit in seen:
                return lo, hi, unit
            seen.add(unit)
            d = qtymod.parse(u["def"], self.units)
            if not d or "/" in d.unit:
                return lo, hi, unit
            lo, hi, unit = lo * d.lo, hi * d.hi, d.unit

    def prop_qty(self, type_, key):
        props = (self.c.ingredients.get(type_) or {}).get("props") or {}
        v = props.get(key)
        if isinstance(v, dict) and isinstance(v.get("value"), str):
            q = qtymod.parse(v["value"], self.units)
            if q and "/" in q.unit:
                num, den = q.unit.split("/")
                lo, hi, nu = self.to_base(q.lo, q.hi, num)
                dlo, dhi, du = self.to_base(1, 1, den)
                return lo / dhi, hi / dlo, nu, du
        return None

    def mass(self, v):
        if v.unknown or v.amount is None:
            return None
        lo, hi, unit = self.to_base(v.amount.lo, v.amount.hi, v.amount.unit)
        dim = self.c.unit_dimension(unit)
        if dim == "Mass":
            return lo, hi, unit
        key = {"Volume": "density", "Count": "unitWeight"}.get(dim)
        conv = self.prop_qty(v.type, key) if key else None
        if not conv or conv[3] != unit:
            return None
        return lo * conv[0], hi * conv[1], conv[2]

    def combine(self, values, label, at, key):
        mix = self.new_value("Mixture")
        dones = [v.state["doneness"] for v in values if not v.unknown and "doneness" in v.state]
        order = self.axis_values("doneness")
        if dones and "doneness" in mix.state:
            mix.state["doneness"] = min(dones, key=lambda x: order.index(x) if x in order else 0)
        traits = set()
        for v in values:
            cl = self.closure(v.traits)
            if "Hazard" in cl:
                traits.add("Hazard")
            traits |= {t for t in cl if t.startswith("Allergen(")}
        mix.traits = traits
        total = [0, 0, None]
        for v in values:
            m = self.mass(v)
            if m is None:
                total = None
                break
            total = [total[0] + m[0], total[1] + m[1], m[2]]
        if total is None:
            mix.amount = None
            self.info("LIN:amountUnknown", at, key, label)
        else:
            mix.amount = qtymod.Quantity(total[0], total[1], total[2] or "g")
        return mix

    def satisfies(self, v, alt):
        if v.unknown:
            return True
        if "type" in alt and v.type != alt["type"]:
            return False
        cl = self.closure(v.traits)
        if not all(t in cl for t in alt.get("traits") or []):
            return False
        for axis, cond in (alt.get("axes") or {}).items():
            if axis not in v.state:
                return False
            cur = v.state[axis]
            order = self.axis_values(axis)
            for op, want in (cond or {}).items():
                if op == "in":
                    name = cur[0] if isinstance(cur, tuple) else cur
                    if name not in want:
                        return False
                elif op in ("atLeast", "atMost"):
                    if cur not in order or want not in order:
                        return False
                    if op == "atLeast" and order.index(cur) < order.index(want):
                        return False
                    if op == "atMost" and order.index(cur) > order.index(want):
                        return False
                elif op == "has":
                    if not isinstance(cur, dict) or want not in cur:
                        return False
        return True

    def finer_reach(self, style):
        enum = ((self.c.axes.get("shape") or {}).get("params") or {}).get("cut", {}).get("style")
        graph = (self.c.enums.get(enum) or {}).get("finer") or {}
        seen, todo = set(), [style]
        while todo:
            n = todo.pop()
            for m in graph.get(n) or []:
                if m not in seen:
                    seen.add(m)
                    todo.append(m)
        return seen

    def apply_effects(self, v, effects, resolved, at, key):
        if v.unknown:
            return
        for axis, eff in (effects or {}).items():
            if axis not in v.state:
                continue
            kind = self.axis_kind(axis)
            order = self.axis_values(axis)
            if kind == "ordered":
                cur = v.state[axis]
                if cur in order and eff in order and order.index(eff) < order.index(cur):
                    continue
                v.state[axis] = eff
                continue
            if isinstance(eff, str):
                name, args = eff, {}
            else:
                name = eff.get("add") or eff.get("set")
                args = {}
                missing = False
                for k, a in (eff.get("args") or {}).items():
                    if isinstance(a, str) and a.startswith("$"):
                        if a[1:] not in resolved:
                            missing = True
                            break
                        args[k] = resolved[a[1:]]
                    else:
                        args[k] = a
                if missing:
                    continue
            if kind == "tagged":
                cur_name, cur_args = v.state[axis]
                if name == "cut":
                    if cur_name == "cut" and args.get("style") not in self.finer_reach(cur_args.get("style")):
                        self.err("TYP:cutCoarser", at, key, cur_args.get("style"), args.get("style"))
                    elif cur_name == "pureed":
                        self.err("TYP:cutCoarser", at, key, "pureed", args.get("style"))
                v.state[axis] = (name, args)
            elif kind == "set":
                v.state[axis][name] = args

    def step(self, rname, i, st, bindings, at, key):
        do = st.get("do")
        act = self.c.actions.get(do)
        args = st.get("args") or []
        named = st.get("named") or {}
        if act is None:
            self.err("NAM:undefined", at, key, do)
            for a in list(args) + list(named.values()):
                for n in (a if isinstance(a, list) else [a]):
                    if isinstance(n, str) and n in bindings:
                        bindings[n].consumed = True
            return UNKNOWN
        params = [p for p in act.get("params") or [] if isinstance(p, dict)]
        assigned = {}
        for j, a in enumerate(args):
            if j < len(params):
                assigned[params[j]["name"]] = a
            else:
                self.err("NAM:arity", at, key, do, "extra", j)
        by_name = {p["name"]: p for p in params}
        for k, a in named.items():
            if k not in by_name:
                self.err("NAM:arity", at, key, do, "unknown", k)
            elif k in assigned:
                self.err("NAM:arity", at, key, do, "duplicate", k)
            else:
                assigned[k] = a
        for p in params:
            if p["name"] not in assigned:
                if "default" in p:
                    assigned[p["name"]] = p["default"]
                elif p.get("kind") in ("food", "foodList", "quantity", "enum"):
                    self.err("NAM:arity", at, key, do, "missing", p["name"])
        label = st.get("let") or do
        values, resolved, inputs = {}, {}, []
        for p in params:
            name = p["name"]
            if name not in assigned:
                continue
            a = assigned[name]
            kind = p.get("kind")
            if kind in ("food", "foodList"):
                names = a if isinstance(a, list) else [a]
                vals = []
                for n in names:
                    if isinstance(n, list):
                        continue
                    e = bindings.get(n)
                    if e is None:
                        self.err("NAM:undefined", at, key, n)
                        vals.append(UNKNOWN)
                        continue
                    if e.consumed:
                        self.err("LIN:doubleUse", at, key, n)
                    e.consumed = True
                    vals.append(e.value.copy())
                inputs.extend(vals)
                if isinstance(a, list) or kind == "foodList":
                    values[name] = (self.combine(vals, label, at, key), "[" + ", ".join(map(str, names)) + "]")
                else:
                    values[name] = (vals[0], str(a))
            elif kind == "quantity":
                q = qtymod.parse(a, self.units)
                if not q:
                    m = re.match(r"^unknown unit (.*)$", q.reason)
                    if m:
                        self.err("NAM:undefined", at, key, m.group(1))
                    else:
                        self.err("TYP:quantity", at, key, name, a)
                    continue
                got = "/".join(str(self.c.unit_dimension(u)) for u in q.units)
                if p.get("dimension") and got != p["dimension"]:
                    self.err("TYP:dimension", at, key, name, p["dimension"], got)
                    continue
                resolved[name] = fmt_amount(q.lo, q.hi, q.unit)
            elif kind == "enum":
                if a not in ((self.c.enums.get(p.get("enum")) or {}).get("values") or []):
                    self.err("TYP:enumValue", at, key, name, a)
                    continue
                resolved[name] = a
        for p in params:
            if p.get("kind") != "food" or p["name"] not in values:
                continue
            v, vname = values[p["name"]]
            alts = p.get("requires") or []
            if alts and not any(self.satisfies(v, alt) for alt in alts):
                self.err("TYP:stateRequirement", at, key, do, p["name"], vname)
            self.apply_effects(v, p.get("effects"), resolved, at, key)
        variants = act.get("variants") or []
        uses = (variants[0].get("uses") if variants else act.get("uses")) or []
        self.requires |= {u for u in uses if u != "hands"}
        if len(variants) > 1:
            self.info("ASM:firstVariant", at, key, *[x.get("name") for x in variants[1:]])
        out = act.get("output")
        if out == "none" or out is None:
            return None
        if "param" in out:
            return values.get(out["param"], (UNKNOWN, ""))[0]
        if out.get("combine"):
            for p in params:
                if p.get("kind") == "foodList" and p["name"] in values:
                    return values[p["name"]][0]
            return UNKNOWN
        res = self.new_value(out["type"], next((v.amount for v in inputs if not v.unknown), None))
        self.apply_effects(res, out.get("effects"), resolved, at, key)
        for v in inputs:
            res.traits |= {t for t in self.closure(v.traits) if t.startswith("Allergen(")}
        return res

    def recipe(self, ri, rname, r):
        bindings = {}
        base = f"recipes.{rname}"
        for ii, (name, spec) in enumerate((r.get("ingredients") or {}).items()):
            at, key = f"{base}.ingredients.{name}", (0, ri, 0, ii)
            bindings[name] = Entry(self.ingredient(rname, name, spec or {}, at, key), at, key)
        for i, st in enumerate(r.get("steps") or []):
            at, key = f"{base}.steps[{i}]", (0, ri, 1, i)
            v = self.step(rname, i, st, bindings, at, key)
            let = st.get("let")
            if let and v is not None:
                if let in bindings:
                    self.err("NAM:rebind", at, key, let)
                bindings[let] = Entry(v, at, key)
                self.steps[f"{rname}.{let}"] = self.render(v)
        at, key = f"{base}.result", (0, ri, 2, 0)
        out = r.get("output") or {}
        e = bindings.get(r.get("result"))
        result = UNKNOWN
        if e is None:
            self.err("NAM:undefined", at, key, r.get("result"))
        else:
            e.returned = True
            result = e.value.copy()
            if not result.unknown:
                if result.type == "Mixture":
                    result.type = out.get("type")
                elif result.type != out.get("type"):
                    self.err("TYP:output", at, key, out.get("type"), result.type)
                result.traits |= set(out.get("traits") or [])
        for name, e in sorted(bindings.items(), key=lambda kv: kv[1].key):
            if not e.consumed and not e.returned:
                self.info("LIN:leftover", e.at, e.key, name)
        return result

    def vessel_traits(self, name):
        out, cur, seen = set(), name, set()
        while cur in self.c.vessels and cur not in seen:
            seen.add(cur)
            v = self.c.vessels.get(cur) or {}
            out |= set(v.get("traits") or [])
            cur = v.get("extends")
        return out

    def menu(self, mi, mname, m, outputs):
        base = f"menus.{mname}"
        bindings = {}
        for ki, (k, rname) in enumerate((m.get("make") or {}).items()):
            at, key = f"{base}.make.{k}", (1, mi, 0, ki, 0)
            if rname not in outputs:
                self.err("NAM:undefined", at, key, rname)
                v = UNKNOWN
            else:
                v = outputs[rname].copy()
            bindings[k] = Entry(v, at, key)
            self.states[f"{mname}.{k}"] = self.render(v)
        for wi, (wave, plates) in enumerate((m.get("serve") or {}).items()):
            for pi, plate in enumerate(plates or []):
                at, key = f"{base}.serve.{wave}[{pi}]", (1, mi, 1, wi, pi)
                vessels = [x for x in plate.get("on") or []]
                for x in vessels:
                    if x not in self.c.vessels:
                        self.err("NAM:undefined", at, key, x)
                for item in plate.get("items") or []:
                    e = bindings.get(item)
                    if e is None:
                        self.err("NAM:undefined", at, key, item)
                        continue
                    if e.consumed:
                        self.err("LIN:doubleUse", at, key, item)
                    e.consumed = True
                    v = e.value
                    if v.unknown:
                        continue
                    cl = self.closure(v.traits)
                    order = self.axis_values("doneness")
                    d = v.state.get("doneness")
                    if "Hazard" in cl and "RawEdible" not in cl and d in order and "done" in order \
                            and order.index(d) < order.index("done"):
                        self.err("SAF:undercooked", at, key, item)
                    if "Liquid" in cl:
                        for x in vessels:
                            if x in self.c.vessels and "HoldsLiquid" not in self.vessel_traits(x):
                                self.err("TYP:vesselLiquid", at, key, item, x)
        for k, e in bindings.items():
            if not e.consumed:
                self.info("LIN:leftover", e.at, e.key, k)

    def render(self, v):
        if v.unknown:
            return {"type": None}
        state = {}
        for a in v.axes:
            cur = v.state.get(a)
            if isinstance(cur, tuple):
                name, args = cur
                params = list((((self.c.axes.get(a) or {}).get("params") or {}).get(name) or {}))
                keys = [k for k in params if k in args] + [k for k in args if k not in params]
                state[a] = name + ("(" + ", ".join(f"{k}: {args[k]}" for k in keys) + ")" if args else "")
            elif isinstance(cur, dict):
                order = self.axis_values(a)
                items = sorted(cur, key=lambda x: order.index(x) if x in order else len(order))
                state[a] = "[" + ", ".join(x + ("(" + ", ".join(f"{k}: {w}" for k, w in cur[x].items()) + ")"
                                               if cur[x] else "") for x in items) + "]"
            else:
                state[a] = cur
        out = {"type": v.type, "state": state, "traits": sorted(v.traits),
               "amount": fmt_amount(v.amount.lo, v.amount.hi, v.amount.unit) if v.amount else "unknown"}
        if v.part:
            out["part"] = v.part
        return out

    def run(self):
        outputs = {}
        for ri, (rname, r) in enumerate((self.doc.get("recipes") or {}).items()):
            outputs[rname] = self.recipe(ri, rname, r or {})
        for mi, (mname, m) in enumerate((self.doc.get("menus") or {}).items()):
            self.menu(mi, mname, m or {}, outputs)
        self.diags.sort(key=lambda d: d["_key"])
        for d in self.diags:
            d.pop("_key")


def count_leaves(x):
    if isinstance(x, dict):
        return sum(count_leaves(v) for v in x.values()) or 1
    if isinstance(x, list):
        return sum(count_leaves(v) for v in x) or 1
    return 1


def compare_expect(expect, result):
    checked = failed = 0
    failures = []

    def unit(label, want, got, weight):
        nonlocal checked, failed
        checked += weight
        if want != got:
            failed += weight
            failures.append({"what": label, "want": want, "got": got})

    for sev, key in (("error", "errors"), ("warning", "warnings"), ("info", "infos")):
        if key not in expect:
            continue
        want = sorted((d["name"], d["at"], [str(a) for a in d.get("args", [])]) for d in expect[key] or [])
        got = sorted((d["name"], d["at"], d["args"]) for d in result["diagnostics"] if d["severity"] == sev)
        unit(key, want, got, count_leaves(expect[key]))
    if "requires" in expect:
        unit("requires", sorted(expect["requires"]), result["requires"], count_leaves(expect["requires"]))
    for group in ("steps", "states"):
        for name, spec in (expect.get(group) or {}).items():
            got = result[group].get(name) or {}
            for field, want in (spec or {}).items():
                g = got.get(field)
                if field == "traits":
                    want = sorted(want or [])
                unit(f"{group}.{name}.{field}", want, g, count_leaves(want) if field != "state" else max(1, len(want)))
    return checked, failed, failures


def run_sample(sample_path, vocab_dir):
    diags = Diags()
    data = load_vocab(vocab_dir, diags)
    c = Checker(data, Diags())
    c.build()
    with open(sample_path, encoding="utf-8") as f:
        text = f.read()
    sdiags = Diags()
    loader = DedupLoader(text)
    try:
        node = loader.get_single_node()
        _dedupe(node, [], os.path.basename(sample_path), sdiags)
        doc = loader.construct_document(node) or {}
    finally:
        loader.dispose()
    with open(os.path.join(ROOT, "spec", "schema", "sample.schema.json"), encoding="utf-8") as f:
        schema = json.load(f)
    result = {"diagnostics": [], "steps": {}, "states": {}, "requires": []}
    for d in sdiags.items:
        result["diagnostics"].append({"severity": "error", "name": d["name"], "at": d["path"], "args": d["args"]})
    errs = list(jsonschema.Draft202012Validator(schema).iter_errors(doc))
    for e in errs:
        result["diagnostics"].append({"severity": "error", "name": "FMT:schema",
                                      "at": fmt_path(list(e.absolute_path)), "args": [e.validator]})
    if not errs:
        s = Sampler(c, doc)
        s.run()
        result["diagnostics"] += s.diags
        result["steps"], result["states"], result["requires"] = s.steps, s.states, sorted(s.requires)
    checked, failed, failures = compare_expect(doc.get("expect") or {}, result) if not errs else (0, 1, [])
    result["expectations"] = {"checked": checked, "failed": failed, "failures": failures}
    return result


def emit(diags, summary, as_json):
    if as_json:
        print(json.dumps({"diagnostics": diags.items, "summary": summary}, ensure_ascii=False, indent=1))
        return
    for d in diags.items:
        print(" ".join([d["severity"], d["name"], d["file"], d["path"] or "-"] + d["args"]), file=sys.stderr)
    print(summary)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="vocabcheck.py")
    ap.add_argument("command", choices=["schema", "check", "lists", "sample"])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--vocab", default=os.path.join(ROOT, "spec", "vocab"))
    ap.add_argument("--json", action="store_true")
    args = ap.parse_intermixed_args(argv)
    diags = Diags()
    if args.command == "lists":
        if len(args.files) != 2:
            ap.error("lists needs CONCEPT_LISTS and CONCEPT_RELATIONS")
        return Lists(load_vocab(args.vocab, diags)).run(*args.files)
    if args.command == "sample":
        if len(args.files) != 1:
            ap.error("sample needs one sample file")
        result = run_sample(args.files[0], args.vocab)
        errors = sum(1 for d in result["diagnostics"] if d["severity"] == "error")
        ex = result["expectations"]
        summary = f"sample: {errors} errors; expectations: {ex['checked']} checked, {ex['failed']} failed"
        if args.json:
            print(json.dumps(dict(result, summary=summary), ensure_ascii=False, indent=1))
        else:
            for d in result["diagnostics"]:
                print(" ".join([d["severity"], d["name"], d["at"] or "-"] + d["args"]), file=sys.stderr)
            for f in ex["failures"]:
                print(f"expectation failed: {f['what']}: want {f['want']!r}, got {f['got']!r}", file=sys.stderr)
            print(summary)
        return 1 if errors or ex["failed"] else 0
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
