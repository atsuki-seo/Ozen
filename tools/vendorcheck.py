# SPDX-License-Identifier: MIT OR Apache-2.0
import hashlib
import os
import sys


def rows(readme):
    lines = open(readme, encoding="utf-8").read().splitlines()
    header = None
    for line in lines:
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if header is None:
            header = cells
            continue
        if set("".join(cells)) <= set("- :"):
            continue
        yield dict(zip(header, cells))


def main(paths):
    files = bad = 0
    for readme in paths:
        if not os.path.exists(readme):
            continue
        base = os.path.dirname(readme)
        for row in rows(readme):
            files += 1
            path = os.path.join(base, row.get("File", ""))
            license_file = os.path.join(base, row.get("License", ""))
            if not os.path.isfile(path):
                print(f"mismatch: {path}: missing", file=sys.stderr)
                bad += 1
                continue
            digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
            if digest != row.get("SHA-256"):
                print(f"mismatch: {path}: sha256 {digest}", file=sys.stderr)
                bad += 1
            elif not os.path.isfile(license_file) or os.path.getsize(license_file) == 0:
                print(f"mismatch: {path}: no license text", file=sys.stderr)
                bad += 1
    print(f"vendor: {files} files, {bad} mismatched")
    return 0 if bad == 0 and files > 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
