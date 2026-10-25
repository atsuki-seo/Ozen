# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import glob
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from collections import Counter

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(ROOT, "tools", "vocabcheck", "vocabcheck.py")
FIXTURES = os.path.join(ROOT, "spec", "fixtures")


def run(*args):
    r = subprocess.run([sys.executable, "-B", CHECKER, *args], capture_output=True, text=True, cwd=ROOT)
    return r


def diag_key(d):
    return (d["severity"], d["name"], d["file"], d["path"], tuple(str(a) for a in d.get("args", [])))


class Schema(unittest.TestCase):
    def test_schema_covers_all_files(self):
        vocab = os.path.join(ROOT, "spec", "vocab")
        files = glob.glob(os.path.join(vocab, "*.yaml"))
        out = run("schema").stdout.strip().splitlines()
        m = re.match(r"^schema: ([0-9]+) files, ([0-9]+) errors$", out[-1] if out else "")
        self.assertIsNotNone(m, out)
        self.assertEqual(int(m.group(1)), len(files))


LISTS = os.path.join(ROOT, "spec", "coverage", "concept-lists.yaml")
RELATIONS = os.path.join(ROOT, "spec", "coverage", "concept-relations.yaml")
LISTS_RE = re.compile(r"^lists: ([0-9]+) listed, ([0-9]+) missing; values: ([0-9]+) checked, ([0-9]+) mismatched; "
                      r"relations: ([0-9]+) listed, ([0-9]+) mismatched$")


class Lists(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="ozen-vocab-")
        self.vocab = os.path.join(self.tmp, "vocab")
        shutil.copytree(os.path.join(ROOT, "spec", "vocab"), self.vocab)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def edit(self, stem, fn):
        path = os.path.join(self.vocab, stem + ".yaml")
        with open(path, encoding="utf-8") as f:
            doc = yaml.safe_load(f)
        fn(doc)
        with open(path, "w", encoding="utf-8") as f:
            yaml.safe_dump(doc, f, allow_unicode=True, sort_keys=False)

    def lists(self):
        r = run("lists", "--vocab", self.vocab, LISTS, RELATIONS)
        m = LISTS_RE.match(r.stdout.strip().splitlines()[-1])
        self.assertIsNotNone(m, r.stdout + r.stderr)
        return tuple(int(x) for x in m.groups())

    def test_lists_reports_missing(self):
        base = self.lists()
        self.assertEqual(base[1], 0)
        self.edit("actions", lambda d: d["actions"].pop("tear"))
        self.assertEqual(self.lists()[1], 1)
        self.setUp()
        self.edit("actions", lambda d: d["actions"].__setitem__("grate", dict(d["actions"]["mix"])))
        self.assertEqual(self.lists()[1], 0)
        self.setUp()

        def swap(d):
            d["actions"]["grate"] = d["actions"].pop("tear")
        self.edit("actions", swap)
        self.assertEqual(self.lists()[1], 1)

    def test_lists_owner(self):
        self.edit("ingredients", lambda d: d["ingredients"]["Beef"]["parts"].pop("belly"))
        self.assertEqual(self.lists()[1], 1)

    def test_lists_values(self):
        self.assertEqual(self.lists()[3], 0)

        def swap_doneness(d):
            v = d["axes"]["doneness"]["values"]
            v[1], v[2] = v[2], v[1]
        self.edit("axes", swap_doneness)
        self.assertEqual(self.lists()[3], 1)
        self.setUp()

        def swap_fallback(d):
            o = d["fallbacks"]["deepPlate"]["order"]
            o[1], o[2] = o[2], o[1]
        self.edit("vessels", swap_fallback)
        self.assertEqual(self.lists()[3], 1)
        self.setUp()
        self.edit("units", lambda d: d["units"]["tbsp"].__setitem__("def", "16 ml"))
        self.assertEqual(self.lists()[3], 1)

    def test_relations_mismatch(self):
        self.assertEqual(self.lists()[5], 0)
        self.edit("capabilities", lambda d: d["kinds"]["Pot"]["provides"].append("RiceCooking"))
        self.assertEqual(self.lists()[5], 1)
        self.setUp()
        self.edit("dirt", lambda d: d["dirt"]["rawFish"]["traits"].remove("Smelly"))
        self.assertEqual(self.lists()[5], 1)
        self.setUp()
        self.edit("ingredients", lambda d: d["ingredients"]["Carrot"]["traits"].remove("SkinEdible"))
        self.assertEqual(self.lists()[5], 1)


class Fixtures(unittest.TestCase):
    maxDiff = None


def vocab_fixture_test(path):
    def test(self):
        with open(os.path.join(path, "expect.yaml"), encoding="utf-8") as f:
            expect = yaml.safe_load(f)
        r = run("check", "--vocab", os.path.join(path, "vocab"), "--json")
        got = json.loads(r.stdout)["diagnostics"]
        self.assertEqual(Counter(map(diag_key, got)), Counter(map(diag_key, expect["diagnostics"])))
        has_error = any(d["severity"] == "error" for d in expect["diagnostics"])
        self.assertEqual(r.returncode, 1 if has_error else 0)
    return test


for path in sorted(glob.glob(os.path.join(FIXTURES, "vocab-*", "*"))):
    kind = os.path.basename(os.path.dirname(path))
    name = os.path.basename(path)
    setattr(Fixtures, "test_fx_" + f"{kind}_{name}".replace("-", "_"), vocab_fixture_test(path))


if __name__ == "__main__":
    unittest.main()
