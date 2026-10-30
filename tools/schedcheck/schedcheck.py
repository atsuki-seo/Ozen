# SPDX-License-Identifier: MIT OR Apache-2.0
import json
import sys
from fractions import Fraction

LEVELS = ["none", "rinse", "wash", "soakThenWash"]


def num(value):
    return Fraction(value["num"], value["den"])


def overlap(a, b):
    return a["start"] < b["endHi"] and b["start"] < a["endHi"]


def check(doc):
    """Return a list of (check, detail) violations of one schedule."""
    out = []
    tasks = {t["id"]: dict(t, start=num(t["start"]), endHi=num(t["endHi"]), endLo=num(t["endLo"]))
             for t in doc["tasks"]}
    resources = {r["id"]: r for r in doc["resources"]}
    waves = {w["id"]: dict(w, serveAt=num(w["serveAt"]), gap=num(w["gap"])) for w in doc.get("waves", [])}

    for t in tasks.values():
        if not t["start"] <= t["endLo"] <= t["endHi"]:
            out.append(("times", f"task {t['id']} ends before it starts"))
        for d in t.get("deps", []):
            if d not in tasks:
                out.append(("deps", f"task {t['id']} depends on unknown task {d}"))
            elif t["start"] < tasks[d]["endHi"]:
                out.append(("deps", f"task {t['id']} starts before task {d} ends"))

    for rid, r in resources.items():
        users = [t for t in tasks.values() if rid in t.get("uses", [])]
        for t in users:
            busy = sum(1 for u in users if u["start"] <= t["start"] < u["endHi"])
            if busy > r["capacity"]:
                out.append(("resources", f"resource {rid} holds {busy} tasks at {t['start']}"))
                break
        if len(r.get("capabilities", [])) > 1:
            for a in users:
                if any(b["id"] > a["id"] and overlap(a, b) for b in users):
                    out.append(("devices", f"device {rid} serves two tasks at once"))
                    break

    cooks = doc["cooks"]
    for t in tasks.values():
        cook = t.get("cook")
        if t.get("unattended") and cook is not None:
            out.append(("people", f"unattended task {t['id']} uses cook {cook}"))
        if cook is not None and not 1 <= cook <= cooks:
            out.append(("people", f"task {t['id']} uses cook {cook} of {cooks}"))
    for a in tasks.values():
        for b in tasks.values():
            if a["id"] < b["id"] and a.get("cook") is not None and a.get("cook") == b.get("cook") and overlap(a, b):
                out.append(("people", f"cook {a['cook']} works on tasks {a['id']} and {b['id']} at once"))

    for w in waves.values():
        for t in tasks.values():
            if t.get("wave") == w["id"] and t["endHi"] > w["serveAt"]:
                out.append(("waves", f"task {t['id']} of wave {w['id']} ends after serving"))
        prev = waves.get(w.get("after"))
        if prev is not None and w["serveAt"] < prev["serveAt"] + w["gap"]:
            out.append(("waves", f"wave {w['id']} is served before wave {prev['id']} plus its gap"))
        if prev is not None and w["gap"] > 0:
            start, end = prev["serveAt"], prev["serveAt"] + w["gap"]
            for t in tasks.values():
                if t.get("cook") is not None and t["start"] < end and t["endHi"] > start:
                    out.append(("meal", f"task {t['id']} uses a cook during the meal after wave {prev['id']}"))

    for dish in doc.get("dishes", []):
        limit = dish.get("serveWithin")
        wave = waves.get(dish.get("wave"))
        ready = tasks.get(dish.get("ready"))
        if limit is not None and wave is not None and ready is not None:
            late = wave["serveAt"] - ready["endHi"] > num(limit)
            if late and not dish.get("missed"):
                out.append(("serving", f"dish {dish['id']} waits longer than allowed"))
            if dish.get("missed") and not late:
                out.append(("serving", f"dish {dish['id']} is marked missed but is served in time"))

    for limit in doc.get("limits", []):
        producer, consumer = tasks.get(limit["producer"]), tasks.get(limit["consumer"])
        if producer is None or consumer is None:
            out.append(("use", f"limit refers to unknown task {limit['producer']} or {limit['consumer']}"))
            continue
        late = consumer["start"] - producer["endHi"] > num(limit["within"])
        if late and not limit.get("missed"):
            out.append(("use", f"task {consumer['id']} uses the value of task {producer['id']} too late"))
        if limit.get("missed") and not late:
            out.append(("use", f"task {consumer['id']} is marked missed but uses the value in time"))

    rules = {r["dirt"]: r["cases"] for r in doc.get("rules", [])}

    def need(mark, later, rid):
        best = 0
        cases = rules.get(mark["dirt"], [])
        for rule in sorted({c["rule"] for c in cases}):
            default, hit = None, None
            for c in (c for c in cases if c["rule"] == rule):
                when = c["when"]
                if when is None:
                    default = c["level"] if default is None else default
                elif hit is not None:
                    continue
                elif when == "sameFoodSameState":
                    if mark.get("food") is not None and mark.get("food") == later.get("food"):
                        hit = c["level"]
                elif when == "sameTool":
                    if any(m["dirt"] == mark["dirt"] and m["resource"] == rid for m in later.get("leaves", [])):
                        hit = c["level"]
                elif when in later.get("targets", []):
                    hit = c["level"]
            level = hit if hit is not None else (default or "none")
            best = max(best, LEVELS.index(level))
        return best

    order = sorted(tasks.values(), key=lambda t: (t["start"], t["id"]))
    history = {}
    cleaned = {}
    used = {}
    for t in order:
        cleans = t.get("cleans")
        cleans = [] if cleans is None else cleans if isinstance(cleans, list) else [cleans]
        cleaning = {c["resource"] for c in cleans}
        for rid in t.get("uses", []):
            if t.get("kind") == "soak" or rid in cleaning:
                continue
            marks = history.get(rid, [])
            level = max((need(m, t, rid) for _, m in marks), default=0)
            done = cleaned.get(rid)
            if level > 0 and (done is None or done < level):
                out.append(("washing", f"task {t['id']} uses {rid} still dirty after task {marks[0][0]}"))
            if done is not None:
                history[rid] = []
                cleaned[rid] = None
            used[rid] = t["id"]
        for clean in cleans:
            rid = clean["resource"]
            cleaned[rid] = 99 if clean["level"] == "wipe" else LEVELS.index(clean["level"])
            used.pop(rid, None)
        for mark in t.get("leaves", []):
            history.setdefault(mark["resource"], []).append((t["id"], mark))

    heat = {}
    for t in order:
        if t.get("kind") == "preheat":
            heat[t["heats"]["resource"]] = num(t["heats"]["temp"])
            continue
        needs = {n["resource"]: num(n["temp"]) for n in t.get("needs", [])}
        for rid in t.get("uses", []):
            if rid not in needs:
                heat[rid] = None
            elif heat.get(rid) is None or heat[rid] < needs[rid]:
                out.append(("preheat", f"task {t['id']} needs {rid} at {needs[rid]} but it is heated to {heat.get(rid)}"))
                heat[rid] = needs[rid]

    cleanup = doc.get("cleanup", "none")
    if cleanup != "none":
        for rid, tid in sorted(used.items()):
            kind = resources.get(rid, {}).get("category")
            if kind in ("tool", "place") or (kind == "vessel" and cleanup == "afterMeal"):
                out.append(("cleanup", f"resource {rid} is still dirty after task {tid}"))
    return out


def main(paths):
    total = 0
    for path in paths:
        with open(path, encoding="utf-8") as f:
            doc = json.load(f)
        for name, detail in check(doc):
            print(f"violation: {path}: {name}: {detail}", file=sys.stderr)
            total += 1
    print(f"schedcheck: {len(paths)} schedules, {total} violations")
    return 0 if total == 0 and paths else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
