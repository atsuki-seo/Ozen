# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import sys

import yaml

HEADER = "// SPDX-License-Identifier: MIT OR Apache-2.0\n"


def string_keys(node):
    # YAML 1.1 reads plain keys such as `on` as booleans; the tables use `on` as a key.
    if isinstance(node, yaml.MappingNode):
        for k, v in node.value:
            if isinstance(k, yaml.ScalarNode):
                k.tag = "tag:yaml.org,2002:str"
            string_keys(v)
    elif isinstance(node, yaml.SequenceNode):
        for v in node.value:
            string_keys(v)


def load(vocab, name):
    with open(os.path.join(vocab, name + ".yaml"), encoding="utf-8") as f:
        loader = yaml.SafeLoader(f.read())
    try:
        node = loader.get_single_node()
        string_keys(node)
        return loader.construct_document(node)
    finally:
        loader.dispose()


def ident(source_id):
    parts = re.split(r"[-_.]", source_id)
    return parts[0] + "".join(p[:1].upper() + p[1:] for p in parts[1:])


def quote(s):
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def sourced(v):
    """A {value, source} pair becomes `value @source(name)`."""
    return f"{v['value']} @source({ident(v['source'])})"


def fct(v):
    return f"fct({ident(v['source'])}, food: {quote(v['food'])}, field: {v['field']})"


def prop_value(v):
    if isinstance(v, dict) and "value" in v:
        return sourced(v)
    if isinstance(v, dict) and "food" in v:
        return fct(v)
    return str(v)


def props_block(props, indent):
    pad = "  " * indent
    lines = [f"{pad}props {{"]
    lines += prop_lines(props, indent + 1)
    lines.append(f"{pad}}}")
    return lines


def prop_lines(props, indent):
    pad = "  " * indent
    out = []
    for k, v in props.items():
        if isinstance(v, dict) and "value" not in v and "food" not in v:
            out.append(f"{pad}{k} {{")
            out += prop_lines(v, indent + 1)
            out.append(f"{pad}}}")
        else:
            out.append(f"{pad}{k}: {prop_value(v)}")
    return out


def parts_block(parts, indent):
    pad = "  " * indent
    lines = [f"{pad}parts {{"]
    for name, spec in parts.items():
        spec = spec or {}
        if not spec:
            lines.append(f"{pad}  {name}")
            continue
        lines.append(f"{pad}  {name} {{")
        if "extends" in spec:
            lines.append(f"{pad}    extends: {spec['extends']}")
        if "traits" in spec:
            lines.append(f"{pad}    traits: {', '.join(spec['traits'])}")
        if spec.get("discard"):
            lines.append(f"{pad}    discard: true")
        if "props" in spec:
            lines += props_block(spec["props"], indent + 2)
        lines.append(f"{pad}  }}")
    lines.append(f"{pad}}}")
    return lines


def decl(head, body):
    if not body:
        return [head]
    return [head + " {"] + body + ["}"]


def dimensions(d):
    return [f"dimension {', '.join(d['dimensions'])}"]


def units(d, prefix=None):
    out = []
    for name, u in d["units"].items():
        if prefix is None and "." in name:
            continue
        if prefix is not None:
            if not name.startswith(prefix + "."):
                continue
            name = name[len(prefix) + 1:]
        value = "base" if u.get("base") else u["def"]
        out.append(f"unit {name} : {u['dimension']} = {value}")
    return out


def value_item(name, params):
    if not params:
        return name
    return f"{name}({', '.join(f'{k}: {v}' for k, v in params.items())})"


def axes(d):
    out = []
    for name, a in d["axes"].items():
        kind = {"ordered": " ordered", "set": " set"}.get(a["kind"], "")
        items = [value_item(v, (a.get("params") or {}).get(v)) for v in a["values"]]
        out.append(f"axis {name} :{kind} {{ {', '.join(items)} }}")
    for name, e in d["enums"].items():
        finer = e.get("finer") or {}
        if not finer:
            out.append(f"enum {name} {{ {', '.join(e['values'])} }}")
            continue
        out.append(f"enum {name} {{")
        out.append(f"  {', '.join(e['values'])}")
        for src, targets in finer.items():
            out.append(f"  finer {src} -> {', '.join(targets)}")
        out.append("}")
    return out


def traits(d):
    out = []
    for name, t in d["traits"].items():
        t = t or {}
        head = f"trait {name}"
        if "params" in t:
            head += "(" + ", ".join(f"{k}: {v}" for k, v in t["params"].items()) + ")"
        if "extends" in t:
            head += " : " + ", ".join(t["extends"])
        body = parts_block(t["parts"], 1) if "parts" in t else []
        out += decl(head, body)
    out.append(f"enum AllergenKind {{ {', '.join(d['allergens'])} }}")
    return out


def capabilities(d):
    out = ["trait Capability", "trait Place", "trait Tool"]
    for name, c in d["capabilities"].items():
        c = c or {}
        body = [f"  modes: {', '.join(c['params'])}"] if "params" in c else []
        out += decl(f"trait {name} : Capability", body)
    for name, k in d["kinds"].items():
        supers = [k["category"].capitalize()] + k.get("extends", []) + k.get("provides", [])
        body = []
        if k.get("abstract"):
            body.append("  abstract: true")
        if "props" in k:
            body += props_block(k["props"], 1)
        out += decl(f"kind {name} : {', '.join(supers)}", body)
    return out


def vessels(d):
    out = []
    for name, v in d["vessels"].items():
        v = v or {}
        supers = ([v["extends"]] if "extends" in v else []) + v.get("traits", [])
        out.append(f"vessel {name}" + (f" : {', '.join(supers)}" if supers else ""))
    for name, f in d["fallbacks"].items():
        args = ", ".join(f["order"]) + (", dryOnly: true" if f.get("dryOnly") else "")
        out.append(f"property {name} = vesselOrder({args})")
    return out


def dirt(d):
    out = [f"dirt {name} : {', '.join(x['traits'])}" for name, x in d["dirt"].items()]
    for name, r in d["rules"].items():
        notes = (" @fixed" if r.get("fixed") else "") + (" @info" if r.get("info") else "")
        cases = [r["level"]] + [f"{c['level']} before {c['when']}" for c in r.get("cases", [])]
        out.append(f"rule {name} after {r['after']}: {', '.join(cases)}{notes}")
    return out


def ingredients(d):
    out = []
    for name, i in d["ingredients"].items():
        head = f"ingredient {name}" + (f" : {', '.join(i['traits'])}" if i.get("traits") else "")
        body = []
        if i.get("structural"):
            body.append("  structural: true")
        if "axes" in i:
            body.append(f"  axes: {', '.join(i['axes']) or '[]'}")
        if "initial" in i:
            body.append("  initial: { " + ", ".join(f"{k}: {v}" for k, v in i["initial"].items()) + " }")
        if "leaves" in i:
            body.append(f"  leaves: {', '.join(i['leaves']) or '[]'}")
        if "props" in i:
            body += props_block(i["props"], 1)
        if "parts" in i:
            body += parts_block(i["parts"], 1)
        out += decl(head, body)
    return out


def aliases(d):
    out = []
    for name, a in d["aliases"].items():
        state = []
        for axis, cond in (a.get("state") or {}).items():
            for value, args in cond.items():
                state.append(f"{axis}: {value}(" + ", ".join(f"{k}: {v}" for k, v in args.items()) + ")")
        alts = [", ".join([p] + state) for p in a.get("parts", [])]
        alts += [", ".join([t] + state) for t in a.get("partTraits", [])]
        out.append(f"alias {name} = {a['of']}<{' | '.join(alts)}>")
    return out


def requirement_alts(p):
    """A food parameter's anyOf list becomes the alternatives of its type arguments."""
    base = "T"
    alts = []
    for alt in p.get("requires", []):
        if "type" in alt:
            base = alt["type"]
        choices = [[]]
        for t in alt.get("traits", []):
            choices = [c + [t] for c in choices]
        for axis, cond in (alt.get("axes") or {}).items():
            if "in" in cond:
                choices = [c + [f"{axis}: {v}"] for c in choices for v in cond["in"]]
            if "has" in cond:
                choices = [c + [f"{axis} has {cond['has']}"] for c in choices]
        alts += [", ".join(c) for c in choices if c]
    return base, alts


def param(p):
    k = p["kind"]
    if k == "food":
        base, alts = requirement_alts(p)
        return f"{p['name']}: {base}" + (f"<{' | '.join(alts)}>" if alts else "")
    if k == "foodList":
        return f"{p['name']}: [T]"
    if k == "quantity":
        return f"{p['name']}: {p['dimension']}" + (f" = {p['default']}" if "default" in p else "")
    if k == "enum":
        return f"{p['name']}: {p['enum']}" + (f" = {p['default']}" if "default" in p else "")
    if k == "tool":
        return f"{p['name']}: Tool"
    if k == "place":
        return f"{p['name']}: Place"
    raise ValueError(f"unknown parameter kind {k}")


def effect_args(e):
    if isinstance(e, dict) and "args" in e:
        return "(" + ", ".join(str(v).lstrip("$") for v in e["args"].values()) + ")"
    return ""


def output(a):
    o = a.get("output")
    if o == "none" or o is None:
        return ""
    if "type" in o:
        return f" -> {o['type']}"
    src = next(p for p in a["params"] if p["name"] == o["param"])
    base, _ = requirement_alts(src)
    effects = []
    for axis, e in (src.get("effects") or {}).items():
        if isinstance(e, dict) and "add" in e:
            effects.append(f"{axis} has {e['add']}{effect_args(e)}")
        elif isinstance(e, dict) and "set" in e:
            effects.append(f"{axis}: {e['set']}{effect_args(e)}")
        else:
            effects.append(f"{axis}: {e}")
    return f" -> {base}" + (f"<{', '.join(effects)}>" if effects else "")


def leaves(items):
    out = []
    for item in items:
        what = item.get("from") or item.get("dirt")
        out += [f"{what} on {target}" for target in item["on"]]
    return "leaves: " + ", ".join(out)


def uses_line(spec, pad):
    uses = spec.get("uses") or []
    notes = (" @unattended" if spec.get("unattended") else "")
    if not uses:
        return []
    line = f"{pad}uses {', '.join(uses)}"
    if "time" in spec:
        line += f" for {spec['time']['value']}{notes} @source({ident(spec['time']['source'])})"
    else:
        line += notes
    return [line]


def actions(d):
    out = []
    for name, a in d["actions"].items():
        head = f"action {name}({', '.join(param(p) for p in a['params'])}){output(a)}"
        if a.get("unattended") and not a.get("uses") and "variants" not in a:
            head += " @unattended"
        body = []
        if a.get("structural"):
            body.append("  structural: true")
        if a.get("output", {}) != "none" and isinstance(a.get("output"), dict) and a["output"].get("combine"):
            body.append("  combine: true")
        body += uses_line(a, "  ")
        if "leaves" in a:
            body.append("  " + leaves(a["leaves"]))
        for v in a.get("variants", []):
            body.append(f"  variant {v['name']} {{")
            body += uses_line(v, "    ")
            if "leaves" in v:
                body.append("    " + leaves(v["leaves"]))
            if "steps" in v:
                body.append(f"    steps {{ {'; '.join(v['steps'])} }}")
            body.append("  }")
        out += decl(head, body)
    return out


def sources(d):
    out = []
    for sid, s in d["sources"].items():
        body = [f"  {k}: {quote(v) if k != 'kind' else v}" for k, v in s.items()]
        out += decl(f"source {ident(sid)}", body)
    return out


KITCHEN_TYPES = """enum Cleanup { none, beforeMeal, afterMeal }
enum Preference { fewerWashes, lessWaiting, fewerTools, steadyPace }
type EquipmentSpec {
  count: Count in 0.. = 1
}
type FloorSpec : EquipmentSpec {
  capacity: Count in 1.. = 1
}
type CounterSpec : EquipmentSpec {
  workSpots: Count in 1.. = 1
}
type StoveSpec : EquipmentSpec {
  burners: Count in 1.. = 1
}
type OvenSpec : EquipmentSpec {
  maxTemp: Temperature?
}
type ContainerSpec : EquipmentSpec {
  capacity: Volume?
}
type FryingPanSpec : EquipmentSpec {
  diameter: Length in 10 cm..40 cm = 26 cm
  capacity: Mass?
}
type RiceCookingSpec : EquipmentSpec {
  capacity: Volume?
}
type VesselSpec : EquipmentSpec {
  capacity: Volume?
}
type Kitchen {
  places: {
    floor: Floor = Floor
    counter: Counter = Counter
    sink: Sink = Sink
  }
  tools: [Tool] = []
  tableware: [Vessel] = []
}
type Person {
  picks: [Dish] = []
}
type Options {
  servings: Count?
  cooks: Count in 1.. = 1
  cleanup: Cleanup = none @needs(kitchen)
  serveAt: Time?
  prefer: [Preference] = []
  people: [Person] = []
  check SCH101 tooManyCooks: cooks <= kitchen.floor.capacity else warn
}
property whileWarm = serveWithin(5..15 min)
property piping = serveWithin(0..3 min)
property beforeMelts = serveWithin(0..5 min)
property justBeforeServe = serveWithin(2 min)
property strict = diagnostics(vagueAmount)
property nominalCutSize = { roughChop: 30 mm, rollCut: 25 mm, biteSize: 25 mm } @source(provisional)
unit cc : Volume = 1 ml @deprecated""".split("\n")


def write(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(HEADER + "\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vocab", default="spec/vocab")
    ap.add_argument("--out", default="stdlib")
    ap.add_argument("--version-file", default="VERSION")
    a = ap.parse_args()
    version = open(a.version_file, encoding="utf-8").read().strip()
    v = {n: load(a.vocab, n) for n in ["dimensions", "units", "axes", "traits", "capabilities", "vessels", "dirt",
                                        "ingredients", "aliases", "actions", "sources"]}
    write(os.path.join(a.out, "package.ozen"), [f"package std {{", f'  version: "{version}"', "}"])
    prelude = (dimensions(v["dimensions"]) + units(v["units"]) + axes(v["axes"]) + traits(v["traits"]) +
               capabilities(v["capabilities"]) + ["resource hands"] + vessels(v["vessels"]) + dirt(v["dirt"]) +
               ingredients(v["ingredients"]) + aliases(v["aliases"]) + actions(v["actions"]) + KITCHEN_TYPES)
    write(os.path.join(a.out, "prelude.ozen"), prelude)
    for region in ["jp", "us"]:
        write(os.path.join(a.out, "units", region + ".ozen"), units(v["units"], f"std.units.{region}"))
    write(os.path.join(a.out, "sources.ozen"), sources(v["sources"]))
    return 0


sys.exit(main())
