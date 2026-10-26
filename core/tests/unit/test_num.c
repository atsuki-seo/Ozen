/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "check.h"
#include "num.h"

static ozni_rat R(int64_t n, int64_t d) {
  ozni_rat r = {0, 1};
  if (ozni_rat_make(n, d, &r) != OZNI_OK) {
    ozn_test_failures++;
  }
  return r;
}

static bool eq(ozni_rat a, int64_t n, int64_t d) { return a.num == n && a.den == d; }

void test_u128_mul(void) {
  ozni_u128 p = ozni_u128_mul64(UINT64_MAX, UINT64_MAX);
  CHECK(p.hi == UINT64_MAX - 1 && p.lo == 1);
  p = ozni_u128_mul64(1ull << 32, 1ull << 32);
  CHECK(p.hi == 1 && p.lo == 0);
  p = ozni_u128_mul64(0xffffffffull, 0x100000001ull);
  CHECK(p.hi == 0 && p.lo == 0xffffffffffffffffull);
}

void test_u128_divmod(void) {
  ozni_u128 q, r;
  ozni_u128 n = ozni_u128_mul64(UINT64_MAX, 1000);
  ozni_u128_divmod(n, ozni_u128_from(1000), &q, &r);
  CHECK(q.hi == 0 && q.lo == UINT64_MAX && ozni_u128_is_zero(r));
  ozni_u128_divmod((ozni_u128){5, 7}, (ozni_u128){1, 0}, &q, &r);
  CHECK(q.hi == 0 && q.lo == 5 && r.hi == 0 && r.lo == 7);
  ozni_u128_divmod(ozni_u128_from(17), ozni_u128_from(5), &q, &r);
  CHECK(q.lo == 3 && r.lo == 2);
}

void test_u128_gcd(void) {
  CHECK(ozni_u128_gcd(ozni_u128_from(84), ozni_u128_from(36)).lo == 12);
  ozni_u128 a = ozni_u128_mul64(1ull << 40, 3), b = ozni_u128_mul64(1ull << 50, 9);
  ozni_u128 g = ozni_u128_gcd(a, b);
  CHECK(g.hi == 0 && g.lo == (1ull << 40) * 3);
  CHECK(ozni_u128_gcd(ozni_u128_from(7), ozni_u128_from(0)).lo == 7);
}

void test_rational_reduce(void) {
  CHECK(eq(R(6, 4), 3, 2));
  CHECK(eq(R(-6, 4), -3, 2));
  CHECK(eq(R(6, -4), -3, 2));
  CHECK(eq(R(0, -5), 0, 1));
  ozni_rat r;
  CHECK(ozni_rat_make(1, 0, &r) == OZNI_DIVZERO);
}

void test_rational_arith(void) {
  ozni_rat r;
  CHECK(ozni_rat_add(R(1, 10), R(2, 10), &r) == OZNI_OK && eq(r, 3, 10));
  CHECK(ozni_rat_sub(R(1, 3), R(1, 2), &r) == OZNI_OK && eq(r, -1, 6));
  CHECK(ozni_rat_mul(R(2, 3), R(-9, 4), &r) == OZNI_OK && eq(r, -3, 2));
  CHECK(ozni_rat_div(R(600, 1), R(250, 1), &r) == OZNI_OK && eq(r, 12, 5));
  CHECK(ozni_rat_div(R(1, 2), R(-1, 4), &r) == OZNI_OK && eq(r, -2, 1));
  CHECK(ozni_rat_div(R(1, 2), R(0, 1), &r) == OZNI_DIVZERO);
  int64_t big = INT64_MAX;
  CHECK(ozni_rat_mul(R(big, 3), R(3, big), &r) == OZNI_OK && eq(r, 1, 1));
  CHECK(ozni_rat_add(R(big - 1, 2), R(1, 2), &r) == OZNI_OK && eq(r, big, 2));
}

void test_rational_cmp_floor_ceil(void) {
  CHECK(ozni_rat_cmp(R(1, 3), R(1, 2)) < 0);
  CHECK(ozni_rat_cmp(R(-1, 3), R(-1, 2)) > 0);
  CHECK(ozni_rat_cmp(R(2, 4), R(1, 2)) == 0);
  CHECK(ozni_rat_cmp(R(INT64_MAX, 1), R(-INT64_MAX, 1)) > 0);
  CHECK(eq(ozni_rat_floor(R(7, 2)), 3, 1));
  CHECK(eq(ozni_rat_floor(R(-7, 2)), -4, 1));
  CHECK(eq(ozni_rat_ceil(R(7, 2)), 4, 1));
  CHECK(eq(ozni_rat_ceil(R(-7, 2)), -3, 1));
  CHECK(eq(ozni_rat_floor(R(-4, 1)), -4, 1));
}

void test_rational_overflow_bounds(void) {
  ozni_rat r;
  CHECK(ozni_rat_add(R(INT64_MAX - 1, 1), R(1, 1), &r) == OZNI_OK && eq(r, INT64_MAX, 1));
  CHECK(ozni_rat_sub(R(-INT64_MAX + 1, 1), R(1, 1), &r) == OZNI_OK && eq(r, -INT64_MAX, 1));
  CHECK(ozni_rat_add(R(INT64_MAX, 1), R(1, 1), &r) == OZNI_OVERFLOW);
  CHECK(ozni_rat_sub(R(-INT64_MAX, 1), R(1, 1), &r) == OZNI_OVERFLOW);
  CHECK(ozni_rat_mul(R(1, INT64_MAX), R(1, 2), &r) == OZNI_OVERFLOW);
  CHECK(ozni_rat_make(INT64_MIN, 1, &r) == OZNI_OVERFLOW);
  CHECK(ozni_rat_make(INT64_MIN, 2, &r) == OZNI_OK && eq(r, INT64_MIN / 2, 1));
}

void test_rational_interval(void) {
  ozni_ival a = {R(1, 1), R(2, 1)}, b = {R(10, 1), R(20, 1)}, r;
  CHECK(ozni_ival_add(a, b, &r) == OZNI_OK && eq(r.lo, 11, 1) && eq(r.hi, 22, 1));
  CHECK(ozni_ival_sub(a, b, &r) == OZNI_OK && eq(r.lo, -19, 1) && eq(r.hi, -8, 1));
  CHECK(ozni_ival_scale(a, R(-3, 1), &r) == OZNI_OK && eq(r.lo, -6, 1) && eq(r.hi, -3, 1));
  CHECK(ozni_ival_div(b, R(-2, 1), &r) == OZNI_OK && eq(r.lo, -10, 1) && eq(r.hi, -5, 1));
  CHECK(ozni_ival_div(b, R(0, 1), &r) == OZNI_DIVZERO);
  CHECK(ozni_ival_is_point((ozni_ival){R(3, 2), R(6, 4)}));
  CHECK(!ozni_ival_is_point(a));
}
