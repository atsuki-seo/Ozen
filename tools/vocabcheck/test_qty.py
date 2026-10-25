# SPDX-License-Identifier: MIT OR Apache-2.0

import sys

sys.dont_write_bytecode = True

import unittest
from fractions import Fraction as F

from tools.vocabcheck.qty import QtyError, Quantity, parse

UNITS = {"g", "kg", "ml", "L", "pcs", "mm", "min", "std.units.jp.cup"}


class Accepts(unittest.TestCase):
    def test_accepts(self):
        cases = {
            "200 g": (F(200), F(200), "g"),
            "0.3..0.5 g": (F(3, 10), F(1, 2), "g"),
            "1/4 pcs": (F(1, 4), F(1, 4), "pcs"),
            "0.6 g/ml": (F(3, 5), F(3, 5), "g/ml"),
            "15 ml": (F(15), F(15), "ml"),
            "4..6 min": (F(4), F(6), "min"),
            "1 std.units.jp.cup": (F(1), F(1), "std.units.jp.cup"),
        }
        for text, (lo, hi, unit) in cases.items():
            with self.subTest(text=text):
                q = parse(text, UNITS)
                self.assertIsInstance(q, Quantity)
                self.assertEqual((q.lo, q.hi, q.unit), (lo, hi, unit))
                self.assertIsInstance(q.lo, F)

    def test_rejects_malformed(self):
        for text in ["200g", "1e3 g", "1/0 g", "g/ml/s", "5.. g", "..5 g", "5 g ", " 5 g",
                     "5  g", "-1 g", "1/0..2 g", "0.5.5 g", "5 g/ml/s", "1,5 g", ""]:
            with self.subTest(text=text):
                q = parse(text, UNITS)
                self.assertIsInstance(q, QtyError)

    def test_rejects_unitless(self):
        for text in ["200", "0.3..0.5", "1/4"]:
            with self.subTest(text=text):
                self.assertIsInstance(parse(text, UNITS), QtyError)

    def test_range_bounds(self):
        self.assertIsInstance(parse("5..3 g", UNITS), QtyError)
        q = parse("5..5 g", UNITS)
        self.assertIsInstance(q, Quantity)
        self.assertEqual((q.lo, q.hi), (F(5), F(5)))
        self.assertTrue(q.is_point)
        self.assertFalse(parse("4..6 min", UNITS).is_point)

    def test_rejects_unknown_unit(self):
        for text in ["5 oz", "5 g/oz", "5 oz/ml", "5 °C"]:
            with self.subTest(text=text):
                self.assertIsInstance(parse(text, UNITS), QtyError)

    def test_non_string_is_error(self):
        for value in [5, 1.5, None, True, ["5 g"]]:
            with self.subTest(value=value):
                self.assertIsInstance(parse(value, UNITS), QtyError)


if __name__ == "__main__":
    unittest.main()
