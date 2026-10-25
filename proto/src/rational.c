/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "num.h"

static uint64_t mag64(int64_t v) { return v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v; }

static ozpi_i128 imul(int64_t a, int64_t b) {
  return (ozpi_i128){(a < 0) != (b < 0) && a != 0 && b != 0, ozpi_u128_mul64(mag64(a), mag64(b))};
}

static ozpi_i128 iadd(ozpi_i128 a, ozpi_i128 b) {
  if (a.neg == b.neg) {
    return (ozpi_i128){a.neg, ozpi_u128_add(a.mag, b.mag)};
  }
  if (ozpi_u128_cmp(a.mag, b.mag) >= 0) {
    ozpi_i128 r = {a.neg, ozpi_u128_sub(a.mag, b.mag)};
    r.neg = r.neg && !ozpi_u128_is_zero(r.mag);
    return r;
  }
  return (ozpi_i128){b.neg, ozpi_u128_sub(b.mag, a.mag)};
}

ozpi_rat ozpi_rat_int(int64_t v) { return (ozpi_rat){v, 1}; }

ozpi_status ozpi_rat_from_i128(ozpi_i128 n, ozpi_u128 d, ozpi_rat *out) {
  if (ozpi_u128_is_zero(d)) {
    return OZPI_DIVZERO;
  }
  if (ozpi_u128_is_zero(n.mag)) {
    *out = (ozpi_rat){0, 1};
    return OZPI_OK;
  }
  ozpi_u128 g = ozpi_u128_gcd(n.mag, d), num, den, rem;
  ozpi_u128_divmod(n.mag, g, &num, &rem);
  ozpi_u128_divmod(d, g, &den, &rem);
  const ozpi_u128 max = ozpi_u128_from((uint64_t)OZPI_RAT_MAX);
  if (ozpi_u128_cmp(num, max) > 0 || ozpi_u128_cmp(den, max) > 0) {
    return OZPI_OVERFLOW;
  }
  int64_t v = (int64_t)num.lo;
  *out = (ozpi_rat){n.neg ? -v : v, (int64_t)den.lo};
  return OZPI_OK;
}

ozpi_status ozpi_rat_make(int64_t n, int64_t d, ozpi_rat *out) {
  if (d == 0) {
    return OZPI_DIVZERO;
  }
  ozpi_i128 num = {(n < 0) != (d < 0) && n != 0, ozpi_u128_from(mag64(n))};
  return ozpi_rat_from_i128(num, ozpi_u128_from(mag64(d)), out);
}

ozpi_status ozpi_rat_add(ozpi_rat a, ozpi_rat b, ozpi_rat *out) {
  ozpi_i128 n = iadd(imul(a.num, b.den), imul(b.num, a.den));
  return ozpi_rat_from_i128(n, ozpi_u128_mul64((uint64_t)a.den, (uint64_t)b.den), out);
}

ozpi_status ozpi_rat_sub(ozpi_rat a, ozpi_rat b, ozpi_rat *out) { return ozpi_rat_add(a, ozpi_rat_neg(b), out); }

ozpi_status ozpi_rat_mul(ozpi_rat a, ozpi_rat b, ozpi_rat *out) {
  return ozpi_rat_from_i128(imul(a.num, b.num), ozpi_u128_mul64((uint64_t)a.den, (uint64_t)b.den), out);
}

ozpi_status ozpi_rat_div(ozpi_rat a, ozpi_rat b, ozpi_rat *out) {
  if (b.num == 0) {
    return OZPI_DIVZERO;
  }
  ozpi_i128 n = imul(a.num, b.den);
  n.neg = n.neg != (b.num < 0) && !ozpi_u128_is_zero(n.mag);
  return ozpi_rat_from_i128(n, ozpi_u128_mul64((uint64_t)a.den, mag64(b.num)), out);
}

ozpi_rat ozpi_rat_neg(ozpi_rat a) { return (ozpi_rat){-a.num, a.den}; }

int ozpi_rat_cmp(ozpi_rat a, ozpi_rat b) {
  ozpi_i128 l = imul(a.num, b.den), r = imul(b.num, a.den);
  if (l.neg != r.neg) {
    return l.neg ? -1 : 1;
  }
  int c = ozpi_u128_cmp(l.mag, r.mag);
  return l.neg ? -c : c;
}

ozpi_rat ozpi_rat_floor(ozpi_rat a) {
  int64_t q = a.num / a.den;
  if (a.num % a.den != 0 && a.num < 0) {
    q--;
  }
  return ozpi_rat_int(q);
}

ozpi_rat ozpi_rat_ceil(ozpi_rat a) {
  int64_t q = a.num / a.den;
  if (a.num % a.den != 0 && a.num > 0) {
    q++;
  }
  return ozpi_rat_int(q);
}

bool ozpi_rat_is_int(ozpi_rat a) { return a.den == 1; }

ozpi_status ozpi_ival_add(ozpi_ival a, ozpi_ival b, ozpi_ival *out) {
  ozpi_status st = ozpi_rat_add(a.lo, b.lo, &out->lo);
  return st != OZPI_OK ? st : ozpi_rat_add(a.hi, b.hi, &out->hi);
}

ozpi_status ozpi_ival_sub(ozpi_ival a, ozpi_ival b, ozpi_ival *out) {
  ozpi_status st = ozpi_rat_sub(a.lo, b.hi, &out->lo);
  return st != OZPI_OK ? st : ozpi_rat_sub(a.hi, b.lo, &out->hi);
}

ozpi_status ozpi_ival_scale(ozpi_ival a, ozpi_rat k, ozpi_ival *out) {
  ozpi_rat x, y;
  ozpi_status st = ozpi_rat_mul(a.lo, k, &x);
  if (st == OZPI_OK) {
    st = ozpi_rat_mul(a.hi, k, &y);
  }
  if (st != OZPI_OK) {
    return st;
  }
  *out = k.num < 0 ? (ozpi_ival){y, x} : (ozpi_ival){x, y};
  return OZPI_OK;
}

ozpi_status ozpi_ival_div(ozpi_ival a, ozpi_rat k, ozpi_ival *out) {
  ozpi_rat x, y;
  ozpi_status st = ozpi_rat_div(a.lo, k, &x);
  if (st == OZPI_OK) {
    st = ozpi_rat_div(a.hi, k, &y);
  }
  if (st != OZPI_OK) {
    return st;
  }
  *out = k.num < 0 ? (ozpi_ival){y, x} : (ozpi_ival){x, y};
  return OZPI_OK;
}

bool ozpi_ival_is_point(ozpi_ival a) { return ozpi_rat_cmp(a.lo, a.hi) == 0; }
