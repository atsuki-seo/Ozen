# SPDX-License-Identifier: MIT OR Apache-2.0
import argparse
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import irprops  # noqa: E402

KITCHENS = {
    "propsSmall": "places {\n    stove: IHStove\n  }\n  tools {\n    pot: Pot\n  }",
    "propsMedium": "places {\n    stove: GasStove { burners: 2 }\n    microwave: Microwave\n  }\n"
                   "  tools {\n    pot: Pot\n    pan: FryingPan\n    board: CuttingBoard\n    knife: Knife\n  }",
    "propsLarge": "places {\n    stove: IHStove { burners: 3 }\n    oven: Oven\n    microwave: Microwave\n"
                  "    toaster: Toaster\n    fridge: Fridge\n  }\n  tools {\n    pot: Pot { count: 2 }\n"
                  "    pan: FryingPan\n    board: CuttingBoard { count: 2 }\n    knife: Knife\n    bowl: MixingBowl\n"
                  "    cooker: RiceCooker\n    blender: Blender\n  }",
}


def extras():
    text = ""
    for k in (1, 2, 3):
        text += f"\nkitchen propsCooks{k} : Options {{\n  cooks: {k}\n}}\n"
    for name, body in KITCHENS.items():
        text += f"\nkitchen {name} {{\n  {body}\n}}\n"
    return text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ozen", required=True)
    ozen = os.path.abspath(ap.parse_args().ozen)
    cases = differences = 0
    with tempfile.TemporaryDirectory() as tmp:
        for rel, text, flags in irprops.corpus():
            if flags or "kitchen." in text or "cooks" in text:
                continue
            irprops.write(tmp, "a.ozen", text + extras())
            base = irprops.ir_lines(ozen, tmp, "a.ozen", [])
            for k in (1, 2, 3):
                for name in KITCHENS:
                    got = irprops.ir_lines(ozen, tmp, "a.ozen", ["--options", f"propsCooks{k}", "--kitchen", name])
                    cases += 1
                    if got != base:
                        differences += 1
                        print(f"difference: {rel} cooks={k} kitchen={name}", file=sys.stderr)
    print(f"ir_invariant: {cases} cases, {differences} differences")
    return 0 if differences == 0 and cases > 0 else 1


sys.exit(main())
