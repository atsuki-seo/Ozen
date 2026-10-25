# SPDX-License-Identifier: MIT OR Apache-2.0

import re
from dataclasses import dataclass
from fractions import Fraction

_NUM = r"(?:[0-9]+/[0-9]+|[0-9]+(?:\.[0-9]+)?)"
_NAME = r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)*"
_QTY = re.compile(rf"^({_NUM})(?:\.\.({_NUM}))? ({_NAME})(?:/({_NAME}))?$")


@dataclass(frozen=True)
class Quantity:
    lo: Fraction
    hi: Fraction
    unit: str

    @property
    def is_point(self):
        return self.lo == self.hi

    @property
    def units(self):
        return tuple(self.unit.split("/"))


@dataclass(frozen=True)
class QtyError:
    text: object
    reason: str

    def __bool__(self):
        return False


def _number(text):
    if "/" in text:
        p, q = text.split("/")
        if int(q) == 0:
            return None
        return Fraction(int(p), int(q))
    return Fraction(text)


def parse(text, units):
    if not isinstance(text, str):
        return QtyError(text, "not a string")
    m = _QTY.match(text)
    if not m:
        if re.match(rf"^{_NUM}(?:\.\.{_NUM})?$", text):
            return QtyError(text, "missing unit")
        return QtyError(text, "malformed quantity")
    lo_s, hi_s, num_unit, den_unit = m.groups()
    lo = _number(lo_s)
    hi = _number(hi_s) if hi_s is not None else lo
    if lo is None or hi is None:
        return QtyError(text, "zero denominator")
    if lo > hi:
        return QtyError(text, "lower bound above upper bound")
    for u in (num_unit, den_unit):
        if u is not None and u not in units:
            return QtyError(text, f"unknown unit {u}")
    unit = num_unit if den_unit is None else f"{num_unit}/{den_unit}"
    return Quantity(lo, hi, unit)
