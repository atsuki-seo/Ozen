# SPDX-License-Identifier: MIT OR Apache-2.0
import json
import os
import re
import sys

SUFFIXES = {".result.json": "result.schema.json"}


def resolve(root, ref):
    if not ref.startswith("#/"):
        raise ValueError(f"unsupported $ref {ref}")
    node = root
    for part in ref[2:].split("/"):
        node = node[part]
    return node


def type_ok(value, name):
    if name == "object":
        return isinstance(value, dict)
    if name == "array":
        return isinstance(value, list)
    if name == "string":
        return isinstance(value, str)
    if name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if name == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if name == "boolean":
        return isinstance(value, bool)
    if name == "null":
        return value is None
    raise ValueError(f"unsupported type {name}")


SUPPORTED = {"$schema", "$id", "id", "$defs", "definitions", "$ref", "title", "description", "default", "format", "type",
             "properties", "required", "additionalProperties", "items", "enum", "const", "oneOf", "anyOf", "minimum",
             "maximum", "minItems", "uniqueItems", "pattern"}


def check(root, schema, value, path, errors):
    unknown = set(schema) - SUPPORTED
    if unknown:
        raise ValueError(f"unsupported keywords {sorted(unknown)}")
    if "$ref" in schema:
        check(root, resolve(root, schema["$ref"]), value, path, errors)
        return
    if "anyOf" in schema:
        if not any(not validate_sub(root, alt, value, path) for alt in schema["anyOf"]):
            errors.append(f"{path}: matches none of anyOf")
        return
    if "oneOf" in schema:
        matches = 0
        for alt in schema["oneOf"]:
            sub = []
            check(root, alt, value, path, sub)
            matches += 0 if sub else 1
        if matches != 1:
            errors.append(f"{path}: matches {matches} of oneOf")
        return
    if "const" in schema and value != schema["const"]:
        errors.append(f"{path}: not {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{path}: {value!r} not in enum")
    types = schema.get("type")
    if types is not None and not any(type_ok(value, n) for n in (types if isinstance(types, list) else [types])):
        errors.append(f"{path}: not {types}")
        return
    if "minimum" in schema and type_ok(value, "number") and value < schema["minimum"]:
        errors.append(f"{path}: below {schema['minimum']}")
    if "maximum" in schema and type_ok(value, "number") and value > schema["maximum"]:
        errors.append(f"{path}: above {schema['maximum']}")
    if "pattern" in schema and isinstance(value, str) and re.search(schema["pattern"], value) is None:
        errors.append(f"{path}: does not match {schema['pattern']}")
    if isinstance(value, list) and len(value) < schema.get("minItems", 0):
        errors.append(f"{path}: fewer than {schema['minItems']} items")
    if isinstance(value, list) and schema.get("uniqueItems"):
        seen = [json.dumps(v, sort_keys=True) for v in value]
        if len(seen) != len(set(seen)):
            errors.append(f"{path}: items are not unique")
    if isinstance(value, dict) and schema.get("type", "object") in ("object", ["object"]):
        props = schema.get("properties", {})
        for key in schema.get("required", []):
            if key not in value:
                errors.append(f"{path}: missing {key}")
        for key, sub in value.items():
            if key in props:
                check(root, props[key], sub, f"{path}.{key}", errors)
            elif schema.get("additionalProperties", True) is False:
                errors.append(f"{path}: unexpected {key}")
    if isinstance(value, list) and "items" in schema:
        for i, item in enumerate(value):
            check(root, schema["items"], item, f"{path}[{i}]", errors)


def validate_sub(root, schema, value, path):
    sub = []
    check(root, schema, value, path, sub)
    return sub


def validate(schema, value):
    errors = []
    check(schema, schema, value, "$", errors)
    return errors


def main(argv):
    if len(argv) >= 2 and argv[0] == "--schema":
        schemas = {"": json.load(open(argv[1], encoding="utf-8"))}
        target = argv[2]
        label = "sarif"
    else:
        schemas = {suffix: json.load(open(os.path.join(argv[0], name), encoding="utf-8"))
                   for suffix, name in SUFFIXES.items()}
        target = argv[1]
        label = "json"
    files = sorted(os.path.join(target, f) for f in os.listdir(target)) if os.path.isdir(target) else [target]
    count = bad = 0
    for path in files:
        schema = next((s for suffix, s in schemas.items() if path.endswith(suffix)), None)
        if schema is None:
            continue
        count += 1
        errors = validate(schema, json.load(open(path, encoding="utf-8")))
        if errors:
            bad += 1
            for e in errors[:5]:
                print(f"invalid: {path}: {e}", file=sys.stderr)
    print(f"{label}: {count} files, {bad} invalid")
    return 0 if bad == 0 and count > 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
