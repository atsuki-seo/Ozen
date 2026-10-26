# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import re
import subprocess
import sys
import tempfile

OPTIONS = """
kitchen propsOptions : Options {
  cooks: 1
}
"""

KITCHEN = """
kitchen propsKitchen {
  places {
    stove: IHStove { burners: 3 }
    oven: Oven
    microwave: Microwave
    toaster: Toaster
    fridge: Fridge
    freezer: Freezer
  }
  tools {
    board: CuttingBoard
    knife: Knife
    peeler: Peeler
    pan: FryingPan
    pot: Pot
    bowl: MixingBowl
    colander: Colander
    chopsticks: CookingChopsticks
    ladle: Ladle
    whisk: Whisk
    spoon: MeasuringSpoon
    cooker: RiceCooker
    blender: Blender
  }
  tableware {
    deep: DeepPlate { count: 8 }
    flat: FlatPlate { count: 8 }
    soup: SoupBowl { count: 8 }
    rice: RiceBowl { count: 8 }
    glass: Glass { count: 8 }
  }
}
"""

LINE = re.compile(r"^(error|warning|info) (\S+) (\S+) (\S+?)(?: args=(\[.*?\]))?(?: via [^:]*:[^:]*)?: ")


def diagnostics(ozen, path, extra):
    out = subprocess.run([ozen, "check", *extra, os.path.basename(path)], cwd=os.path.dirname(path),
                         capture_output=True, text=True).stdout
    found = set()
    for line in out.splitlines():
        m = LINE.match(line)
        if m and not m.group(2).startswith("ASM"):
            found.add((m.group(2), m.group(3), m.group(4), m.group(5) or ""))
    return found


def candidates(root):
    for d, _, files in os.walk(os.path.join(root, "tests", "golden")):
        for f in sorted(files):
            if not f.endswith(".ozen"):
                continue
            path = os.path.join(d, f)
            if not os.path.exists(path[:-5] + ".out"):
                continue
            try:
                text = open(path, encoding="utf-8").read()
            except UnicodeDecodeError:
                continue
            first = text.splitlines()[0] if text else ""
            if first.startswith("// args:") and first.split()[2:3] != ["check"]:
                continue
            if first.startswith("// args:") and len(first.split()) > 3:
                continue
            if "recipe" not in text and "menu" not in text:
                continue
            yield path, text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    a = ap.parse_args()
    ozen = os.path.abspath(a.ozen)
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    cases = decreases = 0
    with tempfile.TemporaryDirectory() as tmp:
        for path, text in sorted(candidates(root)):
            work = os.path.join(tmp, os.path.basename(path))
            with open(work, "w", encoding="utf-8") as f:
                f.write(text + OPTIONS + KITCHEN)
            levels = [diagnostics(ozen, work, []), diagnostics(ozen, work, ["--options", "propsOptions"]),
                      diagnostics(ozen, work, ["--options", "propsOptions", "--kitchen", "propsKitchen"])]
            cases += 1
            for lower, higher in zip(levels, levels[1:]):
                missing = lower - higher
                if missing:
                    decreases += 1
                    print(f"decrease: {os.path.relpath(path, root)}: {sorted(missing)}", file=sys.stderr)
    print(f"monotonic: {cases} cases, {decreases} decreases")
    return 0 if decreases == 0 and cases > 0 else 1


sys.exit(main())
