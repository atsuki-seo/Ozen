# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import glob
import json
import os
import re
import subprocess
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
