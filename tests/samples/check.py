#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR Apache-2.0

import argparse
import json
import os
import subprocess
import sys
from fractions import Fraction

import yaml

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "schedcheck"))
import schedcheck  # noqa: E402


def frac(r):
    return Fraction(r["num"], r["den"])


def matches(item, name, kind_key, name_key):
    return item.get(kind_key) == name or item.get(name_key) == name


def by_name(items, name, default):
    for item in items:
        if item.get("name") == name:
            return item
    return default


class Run:
    def __init__(self, label):
        self.label = label
        self.count = 0
        self.failures = []

    def want(self, what, ok, detail=""):
        self.count += 1
        if not ok:
            self.failures.append(f"{self.label}: {what}{': ' + detail if detail else ''}")


def check_ir(r, ir, spec):
    nodes = ir.get("nodes", []) if ir else []
    labels = {n["label"] for n in nodes}
    by_id = {n["id"]: n for n in nodes}
    for label in spec.get("labels", []):
        r.want(f"ir label {label}", label in labels)
    for kind, least in spec.get("kinds", {}).items():
        got = sum(1 for n in nodes if n["kind"] == kind)
        r.want(f"ir kind {kind}", got >= least, str(got))
    for label, want in spec.get("inputs", {}).items():
        node = next((n for n in nodes if n["label"] == label), None)
        got = [by_id[i["node"]]["label"] for i in node["inputs"] if i["node"] in by_id] if node else None
        r.want(f"ir inputs of {label}", got == want, str(got))
    for label, want in spec.get("allergens", {}).items():
        found = [n for n in nodes if n["label"] == label]
        r.want(f"ir allergens of {label}", any(set(want) <= set(n.get("allergens", [])) for n in found),
               str([n.get("allergens") for n in found]))
    options = [o for c in (ir.get("choices", []) if ir else []) for o in c["options"]]
    for label, count in spec.get("choices", {}).items():
        got = [o["count"] for o in options if o["label"] == label]
        r.want(f"choice {label}", count in got, str(got))
    parents = {n["id"]: [i["node"] for i in n["inputs"]] + list(n.get("after", [])) for n in nodes}
    for item in spec.get("reaches", []):
        for target in [n for n in nodes if matches(n, item["to"], "kind", "label")] or [None]:
            seen, stack = set(), [target["id"]] if target else []
            while stack:
                cur = stack.pop()
                if cur not in seen:
                    seen.add(cur)
                    stack.extend(parents.get(cur, []))
            reached = {n["label"] for n in nodes if n["id"] in seen}
            for source in item["from"]:
                r.want(f"ir {source} reaches {item['to']}", source in reached)
            break


def check_schedule(r, sched, spec):
    tasks = sched.get("tasks", []) if sched else []
    kinds = {res["id"]: res["kind"] for res in (sched.get("resources", []) if sched else [])}

    def named(name):
        op, _, food = name.partition("@")
        return [t for t in tasks if matches(t, op, "kind", "op") and (not food or (t.get("food") or "").split("/")[0] == food)]

    def working(t):
        return bool(t.get("uses")) or t.get("cook") is not None or not t.get("unattended")

    for name in spec.get("tasks", []):
        r.want(f"task {name}", bool(named(name)))
    if "cooks" in spec:
        r.want("cooks", sched is not None and sched.get("cooks") == spec["cooks"], str(sched and sched.get("cooks")))
    if "cooksUsed" in spec:
        used = {t["cook"] for t in tasks if t.get("cook") is not None}
        r.want("cooks used", len(used) == spec["cooksUsed"], str(sorted(used)))
    if "waves" in spec:
        r.want("waves", sched is not None and len(sched.get("waves", [])) == spec["waves"])
    for name, count in spec.get("taskCount", {}).items():
        got = sum(1 for t in named(name) if working(t))
        r.want(f"task count {name}", got == count, str(got))
    for name, wave in spec.get("wave", {}).items():
        r.want(f"wave of {name}", any(t.get("wave") == wave for t in named(name)))
    for name, kind in spec.get("usesKind", []):
        r.want(f"{name} uses {kind}", any(kinds.get(u) == kind for t in named(name) for u in t.get("uses", [])))
    if "serveWithin" in spec:
        got = sum(1 for d in (sched.get("dishes", []) if sched else []) if d.get("serveWithin") is not None)
        r.want("serveWithin dishes", got == spec["serveWithin"], str(got))
    for item, kind in spec.get("holds", []):
        holds = sched.get("holds", []) if sched else []
        r.want(f"hold {item} in {kind}", any(item in h["item"] and kinds.get(h["resource"]) == kind for h in holds))
    for a, b in spec.get("order", []):
        first, then = named(a), named(b)
        ok = bool(first and then) and max(frac(t["endHi"]) for t in first) <= min(frac(t["start"]) for t in then)
        r.want(f"{a} before {b}", ok)


def check_run(r, doc, spec, entry):
    outcome = doc.get("outcome", {})
    for key in ("errors", "warnings"):
        if key in spec:
            r.want(key, outcome.get(key) == spec[key], str(outcome.get(key)))
    if "level" in spec:
        r.want("level", doc.get("level") == spec["level"], str(doc.get("level")))
    diags = doc.get("diagnostics", [])
    names = {d["name"] for d in diags}
    for want in spec.get("diagnostics", {}).get("include", []):
        name, args = (want["name"], [str(a) for a in want.get("args", [])]) if isinstance(want, dict) else (want, [])
        r.want(f"diagnostic {name} {args}" if args else f"diagnostic {name}",
               any(d["name"] == name and d.get("args", [])[:len(args)] == args for d in diags))
    for name in spec.get("diagnostics", {}).get("exclude", []):
        r.want(f"no diagnostic {name}", name not in names)
    irs = doc.get("ir", [])
    schedules = doc.get("schedules", [])
    if "ir" in spec:
        check_ir(r, by_name(irs, entry, irs[0] if irs else None), spec["ir"])
    if "schedule" in spec:
        found = by_name(schedules, entry, schedules[-1] if schedules else None)
        check_schedule(r, found["schedule"] if found else None, spec["schedule"])
    for s in schedules:
        violations = list(schedcheck.check(s["schedule"]))
        r.want(f"schedcheck {s['name']}", not violations, "; ".join(f"{n}: {d}" for n, d in violations))


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--ozen", required=True)
    parser.add_argument("dir")
    args = parser.parse_args(argv)
    ozen = os.path.abspath(args.ozen)
    samples, expectations, failures = 0, 0, []
    for name in sorted(os.listdir(args.dir)):
        sample = os.path.join(args.dir, name)
        path = os.path.join(sample, "expect.yaml")
        if not os.path.isfile(path):
            continue
        samples += 1
        with open(path, encoding="utf-8") as f:
            expect = yaml.safe_load(f) or {}
        entry = expect.get("entry")
        for run in expect.get("runs", []):
            r = Run(f"{name}/{run.get('name', '?')}")
            cmd = [ozen, "check", "--format", "json", "--emit", "ir,schedule,steps"]
            if run.get("kitchen"):
                cmd += ["--kitchen", run["kitchen"]]
            if run.get("options"):
                cmd += ["--options", run["options"]]
            if run.get("cooks") is not None:
                cmd += ["--cooks", str(run["cooks"])]
            if entry:
                cmd += ["--entry", entry]
            cmd += [str(a) for a in run.get("args", [])]
            proc = subprocess.run(cmd, cwd=sample, capture_output=True, timeout=60)
            try:
                doc = json.loads(proc.stdout.decode("utf-8"))
            except ValueError:
                doc = None
            r.want("json output", isinstance(doc, dict), proc.stderr.decode("utf-8", "replace").strip()[:200])
            if isinstance(doc, dict):
                check_run(r, doc, run.get("expect", {}), entry)
            expectations += r.count
            failures += r.failures
    for line in failures:
        print(f"FAIL {line}", file=sys.stderr)
    print(f"samples: {samples} samples, {expectations} expectations, {len(failures)} failed")
    return 0 if samples > 0 and not failures else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
