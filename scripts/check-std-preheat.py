# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import json
import subprocess
import sys


def props(kind):
    names = set()
    for member in kind.get("members", []):
        if member.get("section") == "props":
            names.update(m["field"] for m in member.get("members", []) if "field" in m)
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    a = ap.parse_args()
    out = subprocess.run([a.ozen, "debug", "definitions"], capture_output=True, text=True).stdout
    kinds = {d["name"]: d for d in json.loads(out)["definitions"]
             if d.get("kind") == "kind" and d.get("module", "").startswith("std.")}

    def is_oven(name, seen=()):
        k = kinds.get(name)
        if k is None or name in seen:
            return False
        for s in k.get("supers", []):
            if s.get("name") == "HeatSource" and any({"bare": "oven"} in alt for alt in s.get("alts", [])):
                return True
            if is_oven(s.get("name"), seen + (name,)):
                return True
        return False

    def has_preheat(name, seen=()):
        k = kinds.get(name)
        if k is None or name in seen:
            return False
        return "preheatTime" in props(k) or any(has_preheat(s.get("name"), seen + (name,)) for s in k.get("supers", []))

    ovens = sorted(n for n, k in kinds.items() if is_oven(n) and not any(
        m.get("field") == "abstract" for m in k.get("members", [])))
    with_time = [n for n in ovens if has_preheat(n)]
    for n in ovens:
        if n not in with_time:
            print(f"no preheatTime: {n}", file=sys.stderr)
    print(f"oven kinds: {len(ovens)}, with preheatTime: {len(with_time)}")
    return 0 if ovens and len(with_time) == len(ovens) else 1


sys.exit(main())
