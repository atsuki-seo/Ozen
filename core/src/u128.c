/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "num.h"

ozni_u128 ozni_u128_from(uint64_t v) { return (ozni_u128){0, v}; }

ozni_u128 ozni_u128_add(ozni_u128 a, ozni_u128 b) {
  ozni_u128 r = {a.hi + b.hi, a.lo + b.lo};
  if (r.lo < a.lo) {
    r.hi++;
  }
  return r;
}

ozni_u128 ozni_u128_sub(ozni_u128 a, ozni_u128 b) {
  ozni_u128 r = {a.hi - b.hi, a.lo - b.lo};
  if (a.lo < b.lo) {
    r.hi--;
  }
  return r;
}

ozni_u128 ozni_u128_mul64(uint64_t a, uint64_t b) {
  uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
  uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
  uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
  ozni_u128 r;
  r.lo = (p00 & 0xffffffffu) | (mid << 32);
  r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  return r;
}

int ozni_u128_cmp(ozni_u128 a, ozni_u128 b) {
  if (a.hi != b.hi) {
    return a.hi < b.hi ? -1 : 1;
  }
  if (a.lo != b.lo) {
    return a.lo < b.lo ? -1 : 1;
  }
  return 0;
}

bool ozni_u128_is_zero(ozni_u128 a) { return a.hi == 0 && a.lo == 0; }

static ozni_u128 shl1(ozni_u128 a) { return (ozni_u128){(a.hi << 1) | (a.lo >> 63), a.lo << 1}; }

void ozni_u128_divmod(ozni_u128 n, ozni_u128 d, ozni_u128 *q, ozni_u128 *r) {
  ozni_u128 quo = {0, 0}, rem = {0, 0};
  for (int i = 127; i >= 0; i--) {
    rem = shl1(rem);
    uint64_t bit = i >= 64 ? (n.hi >> (i - 64)) & 1u : (n.lo >> i) & 1u;
    rem.lo |= bit;
    quo = shl1(quo);
    if (ozni_u128_cmp(rem, d) >= 0) {
      rem = ozni_u128_sub(rem, d);
      quo.lo |= 1u;
    }
  }
  *q = quo;
  *r = rem;
}

ozni_u128 ozni_u128_gcd(ozni_u128 a, ozni_u128 b) {
  while (!ozni_u128_is_zero(b)) {
    ozni_u128 q, r;
    ozni_u128_divmod(a, b, &q, &r);
    a = b;
    b = r;
  }
  return a;
}
