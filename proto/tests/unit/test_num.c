/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "check.h"
#include "num.h"

static ozpi_rat R(int64_t n, int64_t d) {
  ozpi_rat r = {0, 1};
  if (ozpi_rat_make(n, d, &r) != OZPI_OK) {
    ozp_test_failures++;
  }
  return r;
}

static bool eq(ozpi_rat a, int64_t n, int64_t d) { return a.num == n && a.den == d; }

void test_u128_mul(void) {
  ozpi_u128 p = ozpi_u128_mul64(UINT64_MAX, UINT64_MAX);
  CHECK(p.hi == UINT64_MAX - 1 && p.lo == 1);
  p = ozpi_u128_mul64(1ull << 32, 1ull << 32);
  CHECK(p.hi == 1 && p.lo == 0);
  p = ozpi_u128_mul64(0xffffffffull, 0x100000001ull);
  CHECK(p.hi == 0 && p.lo == 0xffffffffffffffffull);
}

void test_u128_divmod(void) {
  ozpi_u128 q, r;
  ozpi_u128 n = ozpi_u128_mul64(UINT64_MAX, 1000);
  ozpi_u128_divmod(n, ozpi_u128_from(1000), &q, &r);
  CHECK(q.hi == 0 && q.lo == UINT64_MAX && ozpi_u128_is_zero(r));
  ozpi_u128_divmod((ozpi_u128){5, 7}, (ozpi_u128){1, 0}, &q, &r);
  CHECK(q.hi == 0 && q.lo == 5 && r.hi == 0 && r.lo == 7);
  ozpi_u128_divmod(ozpi_u128_from(17), ozpi_u128_from(5), &q, &r);
  CHECK(q.lo == 3 && r.lo == 2);
}

void test_u128_gcd(void) {
  CHECK(ozpi_u128_gcd(ozpi_u128_from(84), ozpi_u128_from(36)).lo == 12);
  ozpi_u128 a = ozpi_u128_mul64(1ull << 40, 3), b = ozpi_u128_mul64(1ull << 50, 9);
  ozpi_u128 g = ozpi_u128_gcd(a, b);
  CHECK(g.hi == 0 && g.lo == (1ull << 40) * 3);
  CHECK(ozpi_u128_gcd(ozpi_u128_from(7), ozpi_u128_from(0)).lo == 7);
}

void test_rational_reduce(void) {
  CHECK(eq(R(6, 4), 3, 2));
  CHECK(eq(R(-6, 4), -3, 2));
  CHECK(eq(R(6, -4), -3, 2));
  CHECK(eq(R(0, -5), 0, 1));
  ozpi_rat r;
  CHECK(ozpi_rat_make(1, 0, &r) == OZPI_DIVZERO);
}

void test_rational_arith(void) {
  ozpi_rat r;
  CHECK(ozpi_rat_add(R(1, 10), R(2, 10), &r) == OZPI_OK && eq(r, 3, 10));
  CHECK(ozpi_rat_sub(R(1, 3), R(1, 2), &r) == OZPI_OK && eq(r, -1, 6));
  CHECK(ozpi_rat_mul(R(2, 3), R(-9, 4), &r) == OZPI_OK && eq(r, -3, 2));
  CHECK(ozpi_rat_div(R(600, 1), R(250, 1), &r) == OZPI_OK && eq(r, 12, 5));
  CHECK(ozpi_rat_div(R(1, 2), R(-1, 4), &r) == OZPI_OK && eq(r, -2, 1));
  CHECK(ozpi_rat_div(R(1, 2), R(0, 1), &r) == OZPI_DIVZERO);
  int64_t big = INT64_MAX;
  CHECK(ozpi_rat_mul(R(big, 3), R(3, big), &r) == OZPI_OK && eq(r, 1, 1));
  CHECK(ozpi_rat_add(R(big - 1, 2), R(1, 2), &r) == OZPI_OK && eq(r, big, 2));
}

void test_rational_cmp_floor_ceil(void) {
  CHECK(ozpi_rat_cmp(R(1, 3), R(1, 2)) < 0);
  CHECK(ozpi_rat_cmp(R(-1, 3), R(-1, 2)) > 0);
  CHECK(ozpi_rat_cmp(R(2, 4), R(1, 2)) == 0);
  CHECK(ozpi_rat_cmp(R(INT64_MAX, 1), R(-INT64_MAX, 1)) > 0);
  CHECK(eq(ozpi_rat_floor(R(7, 2)), 3, 1));
  CHECK(eq(ozpi_rat_floor(R(-7, 2)), -4, 1));
  CHECK(eq(ozpi_rat_ceil(R(7, 2)), 4, 1));
  CHECK(eq(ozpi_rat_ceil(R(-7, 2)), -3, 1));
  CHECK(eq(ozpi_rat_floor(R(-4, 1)), -4, 1));
}

void test_rational_overflow_bounds(void) {
  ozpi_rat r;
  CHECK(ozpi_rat_add(R(INT64_MAX - 1, 1), R(1, 1), &r) == OZPI_OK && eq(r, INT64_MAX, 1));
  CHECK(ozpi_rat_sub(R(-INT64_MAX + 1, 1), R(1, 1), &r) == OZPI_OK && eq(r, -INT64_MAX, 1));
  CHECK(ozpi_rat_add(R(INT64_MAX, 1), R(1, 1), &r) == OZPI_OVERFLOW);
  CHECK(ozpi_rat_sub(R(-INT64_MAX, 1), R(1, 1), &r) == OZPI_OVERFLOW);
  CHECK(ozpi_rat_mul(R(1, INT64_MAX), R(1, 2), &r) == OZPI_OVERFLOW);
  CHECK(ozpi_rat_make(INT64_MIN, 1, &r) == OZPI_OVERFLOW);
  CHECK(ozpi_rat_make(INT64_MIN, 2, &r) == OZPI_OK && eq(r, INT64_MIN / 2, 1));
}

void test_rational_interval(void) {
  ozpi_ival a = {R(1, 1), R(2, 1)}, b = {R(10, 1), R(20, 1)}, r;
  CHECK(ozpi_ival_add(a, b, &r) == OZPI_OK && eq(r.lo, 11, 1) && eq(r.hi, 22, 1));
  CHECK(ozpi_ival_sub(a, b, &r) == OZPI_OK && eq(r.lo, -19, 1) && eq(r.hi, -8, 1));
  CHECK(ozpi_ival_scale(a, R(-3, 1), &r) == OZPI_OK && eq(r.lo, -6, 1) && eq(r.hi, -3, 1));
  CHECK(ozpi_ival_div(b, R(-2, 1), &r) == OZPI_OK && eq(r.lo, -10, 1) && eq(r.hi, -5, 1));
  CHECK(ozpi_ival_div(b, R(0, 1), &r) == OZPI_DIVZERO);
  CHECK(ozpi_ival_is_point((ozpi_ival){R(3, 2), R(6, 4)}));
  CHECK(!ozpi_ival_is_point(a));
}
