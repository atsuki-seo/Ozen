# SPDX-License-Identifier: MIT OR Apache-2.0
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

NS = {"h": "http://www.w3.org/1999/xhtml", "s": "http://www.w3.org/2000/svg"}


def parse(text):
    return ET.fromstring(re.sub(r"^\s*<!DOCTYPE[^>]*>", "", text, flags=re.I))


def local(tag):
    return tag.rsplit("}", 1)[-1]


PAGE_WIDTH = 720


def check_gantt(svg, scalable):
    width, height = float(svg.get("width", "0")), float(svg.get("height", "0"))
    label, per_minute = float(svg.get("data-label-width", "nan")), float(svg.get("data-per-minute", "nan"))
    rows, overlaps, outside = {}, 0, 0
    if width > PAGE_WIDTH and not scalable:
        outside += 1
    for rect in svg.iter():
        if local(rect.tag) != "rect" or rect.get("data-row") is None:
            continue
        start, end = float(rect.get("data-start")), float(rect.get("data-end"))
        x, w = float(rect.get("x")), float(rect.get("width"))
        y, h = float(rect.get("y", "0")), float(rect.get("height", "0"))
        placed = abs(x - (label + start / 60 * per_minute)) <= 0.1 and abs(w - max(1, (end - start) / 60 * per_minute)) <= 0.1
        if x < 0 or x + w > width + 0.05 or y < 0 or y + h > height or end < start or not placed:
            outside += 1
        rows.setdefault(rect.get("data-row"), []).append((start, end))
    for spans in rows.values():
        spans.sort()
        for (a0, a1), (b0, b1) in zip(spans, spans[1:]):
            if b0 < a1:
                overlaps += 1
    return overlaps, outside


def dimensions(path):
    try:
        defs = json.load(open(path, encoding="utf-8"))["definitions"]
    except (OSError, ValueError, KeyError):
        return set()
    return {d["name"] for d in defs if d.get("kind") == "dimension" and d.get("name") != "Count"}


def check_form(root, dims):
    labelled = {e.get("for") for e in root.iter() if local(e.tag) == "label"}
    inputs = unlabelled = no_unit = 0
    for e in root.iter():
        if local(e.tag) not in ("input", "select", "textarea") or e.get("type") in ("hidden", "submit", "button"):
            continue
        inputs += 1
        if e.get("id") not in labelled:
            unlabelled += 1
        quantity = (e.get("data-type") or "").rstrip("?") in dims or e.get("data-quantity") == "true"
        if quantity and not e.get("data-unit"):
            no_unit += 1
    return inputs, unlabelled, no_unit


def main(argv):
    definitions = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "build", "definitions.json")
    if len(argv) == 3 and argv[0] == "--definitions":
        definitions, argv = argv[1], argv[2:]
    if len(argv) != 1:
        print("usage: htmlcheck.py [--definitions FILE] HTML_DIR", file=sys.stderr)
        return 2
    dims = dimensions(definitions)
    files = broken = charts = overlaps = outside = inputs = unlabelled = no_unit = 0
    for name in sorted(os.listdir(argv[0])) if os.path.isdir(argv[0]) else []:
        if not name.endswith(".html"):
            continue
        files += 1
        try:
            root = parse(open(os.path.join(argv[0], name), encoding="utf-8").read())
        except ET.ParseError as e:
            broken += 1
            print(f"not well-formed: {name}: {e}", file=sys.stderr)
            continue
        styles = " ".join(e.text or "" for e in root.iter() if local(e.tag) == "style")
        scalable = re.search(r"(^|[}\s])svg\s*\{[^}]*max-width:\s*100%", styles) is not None
        for svg in root.iter():
            if local(svg.tag) == "svg" and svg.get("data-gantt") == "true":
                charts += 1
                o, x = check_gantt(svg, scalable)
                overlaps, outside = overlaps + o, outside + x
                if o or x:
                    print(f"gantt: {name}: {o} overlaps, {x} out of bounds", file=sys.stderr)
        i, u, n = check_form(root, dims)
        inputs, unlabelled, no_unit = inputs + i, unlabelled + u, no_unit + n
    print(f"html: {files} files, {broken} not well-formed; gantt: {charts} charts, {overlaps} overlaps, "
          f"{outside} out of bounds; form: {inputs} inputs, {unlabelled} without label, {no_unit} quantity without unit")
    return 0 if files > 0 and broken + overlaps + outside + unlabelled + no_unit == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
