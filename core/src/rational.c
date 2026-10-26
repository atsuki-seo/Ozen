/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "num.h"

static uint64_t mag64(int64_t v) { return v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v; }

static ozni_i128 imul(int64_t a, int64_t b) {
  return (ozni_i128){(a < 0) != (b < 0) && a != 0 && b != 0, ozni_u128_mul64(mag64(a), mag64(b))};
}

static ozni_i128 iadd(ozni_i128 a, ozni_i128 b) {
  if (a.neg == b.neg) {
    return (ozni_i128){a.neg, ozni_u128_add(a.mag, b.mag)};
  }
  if (ozni_u128_cmp(a.mag, b.mag) >= 0) {
    ozni_i128 r = {a.neg, ozni_u128_sub(a.mag, b.mag)};
    r.neg = r.neg && !ozni_u128_is_zero(r.mag);
    return r;
  }
  return (ozni_i128){b.neg, ozni_u128_sub(b.mag, a.mag)};
}

ozni_rat ozni_rat_int(int64_t v) { return (ozni_rat){v, 1}; }

ozni_status ozni_rat_from_i128(ozni_i128 n, ozni_u128 d, ozni_rat *out) {
  if (ozni_u128_is_zero(d)) {
    return OZNI_DIVZERO;
  }
  if (ozni_u128_is_zero(n.mag)) {
    *out = (ozni_rat){0, 1};
    return OZNI_OK;
  }
  ozni_u128 g = ozni_u128_gcd(n.mag, d), num, den, rem;
  ozni_u128_divmod(n.mag, g, &num, &rem);
  ozni_u128_divmod(d, g, &den, &rem);
  const ozni_u128 max = ozni_u128_from((uint64_t)OZNI_RAT_MAX);
  if (ozni_u128_cmp(num, max) > 0 || ozni_u128_cmp(den, max) > 0) {
    return OZNI_OVERFLOW;
  }
  int64_t v = (int64_t)num.lo;
  *out = (ozni_rat){n.neg ? -v : v, (int64_t)den.lo};
  return OZNI_OK;
}

ozni_status ozni_rat_make(int64_t n, int64_t d, ozni_rat *out) {
  if (d == 0) {
    return OZNI_DIVZERO;
  }
  ozni_i128 num = {(n < 0) != (d < 0) && n != 0, ozni_u128_from(mag64(n))};
  return ozni_rat_from_i128(num, ozni_u128_from(mag64(d)), out);
}

ozni_status ozni_rat_add(ozni_rat a, ozni_rat b, ozni_rat *out) {
  ozni_i128 n = iadd(imul(a.num, b.den), imul(b.num, a.den));
  return ozni_rat_from_i128(n, ozni_u128_mul64((uint64_t)a.den, (uint64_t)b.den), out);
}

ozni_status ozni_rat_sub(ozni_rat a, ozni_rat b, ozni_rat *out) { return ozni_rat_add(a, ozni_rat_neg(b), out); }

ozni_status ozni_rat_mul(ozni_rat a, ozni_rat b, ozni_rat *out) {
  return ozni_rat_from_i128(imul(a.num, b.num), ozni_u128_mul64((uint64_t)a.den, (uint64_t)b.den), out);
}

ozni_status ozni_rat_div(ozni_rat a, ozni_rat b, ozni_rat *out) {
  if (b.num == 0) {
    return OZNI_DIVZERO;
  }
  ozni_i128 n = imul(a.num, b.den);
  n.neg = n.neg != (b.num < 0) && !ozni_u128_is_zero(n.mag);
  return ozni_rat_from_i128(n, ozni_u128_mul64((uint64_t)a.den, mag64(b.num)), out);
}

ozni_rat ozni_rat_neg(ozni_rat a) { return (ozni_rat){-a.num, a.den}; }

int ozni_rat_cmp(ozni_rat a, ozni_rat b) {
  ozni_i128 l = imul(a.num, b.den), r = imul(b.num, a.den);
  if (l.neg != r.neg) {
    return l.neg ? -1 : 1;
  }
  int c = ozni_u128_cmp(l.mag, r.mag);
  return l.neg ? -c : c;
}

ozni_rat ozni_rat_floor(ozni_rat a) {
  int64_t q = a.num / a.den;
  if (a.num % a.den != 0 && a.num < 0) {
    q--;
  }
  return ozni_rat_int(q);
}

ozni_rat ozni_rat_ceil(ozni_rat a) {
  int64_t q = a.num / a.den;
  if (a.num % a.den != 0 && a.num > 0) {
    q++;
  }
  return ozni_rat_int(q);
}

bool ozni_rat_is_int(ozni_rat a) { return a.den == 1; }

ozni_status ozni_ival_add(ozni_ival a, ozni_ival b, ozni_ival *out) {
  ozni_status st = ozni_rat_add(a.lo, b.lo, &out->lo);
  return st != OZNI_OK ? st : ozni_rat_add(a.hi, b.hi, &out->hi);
}

ozni_status ozni_ival_sub(ozni_ival a, ozni_ival b, ozni_ival *out) {
  ozni_status st = ozni_rat_sub(a.lo, b.hi, &out->lo);
  return st != OZNI_OK ? st : ozni_rat_sub(a.hi, b.lo, &out->hi);
}

ozni_status ozni_ival_scale(ozni_ival a, ozni_rat k, ozni_ival *out) {
  ozni_rat x, y;
  ozni_status st = ozni_rat_mul(a.lo, k, &x);
  if (st == OZNI_OK) {
    st = ozni_rat_mul(a.hi, k, &y);
  }
  if (st != OZNI_OK) {
    return st;
  }
  *out = k.num < 0 ? (ozni_ival){y, x} : (ozni_ival){x, y};
  return OZNI_OK;
}

ozni_status ozni_ival_div(ozni_ival a, ozni_rat k, ozni_ival *out) {
  ozni_rat x, y;
  ozni_status st = ozni_rat_div(a.lo, k, &x);
  if (st == OZNI_OK) {
    st = ozni_rat_div(a.hi, k, &y);
  }
  if (st != OZNI_OK) {
    return st;
  }
  *out = k.num < 0 ? (ozni_ival){y, x} : (ozni_ival){x, y};
  return OZNI_OK;
}

bool ozni_ival_is_point(ozni_ival a) { return ozni_rat_cmp(a.lo, a.hi) == 0; }
