/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_NUM_H
#define OZNI_NUM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ozni_u128 {
  uint64_t hi, lo;
} ozni_u128;

typedef struct ozni_i128 {
  bool neg;
  ozni_u128 mag;
} ozni_i128;

ozni_u128 ozni_u128_from(uint64_t v);
ozni_u128 ozni_u128_add(ozni_u128 a, ozni_u128 b);
ozni_u128 ozni_u128_sub(ozni_u128 a, ozni_u128 b);
ozni_u128 ozni_u128_mul64(uint64_t a, uint64_t b);
int ozni_u128_cmp(ozni_u128 a, ozni_u128 b);
bool ozni_u128_is_zero(ozni_u128 a);
void ozni_u128_divmod(ozni_u128 n, ozni_u128 d, ozni_u128 *q, ozni_u128 *r);
ozni_u128 ozni_u128_gcd(ozni_u128 a, ozni_u128 b);

typedef struct ozni_rat {
  int64_t num;
  int64_t den;
} ozni_rat;

typedef enum ozni_status : uint8_t { OZNI_OK, OZNI_OVERFLOW, OZNI_DIVZERO } ozni_status;

constexpr int64_t OZNI_RAT_MAX = INT64_MAX;

ozni_rat ozni_rat_int(int64_t v);
[[nodiscard]] ozni_status ozni_rat_from_i128(ozni_i128 n, ozni_u128 d, ozni_rat *out);
[[nodiscard]] ozni_status ozni_rat_make(int64_t n, int64_t d, ozni_rat *out);
[[nodiscard]] ozni_status ozni_rat_add(ozni_rat a, ozni_rat b, ozni_rat *out);
[[nodiscard]] ozni_status ozni_rat_sub(ozni_rat a, ozni_rat b, ozni_rat *out);
[[nodiscard]] ozni_status ozni_rat_mul(ozni_rat a, ozni_rat b, ozni_rat *out);
[[nodiscard]] ozni_status ozni_rat_div(ozni_rat a, ozni_rat b, ozni_rat *out);
ozni_rat ozni_rat_neg(ozni_rat a);
int ozni_rat_cmp(ozni_rat a, ozni_rat b);
ozni_rat ozni_rat_floor(ozni_rat a);
ozni_rat ozni_rat_ceil(ozni_rat a);
bool ozni_rat_is_int(ozni_rat a);

typedef struct ozni_ival {
  ozni_rat lo, hi;
} ozni_ival;

[[nodiscard]] ozni_status ozni_ival_add(ozni_ival a, ozni_ival b, ozni_ival *out);
[[nodiscard]] ozni_status ozni_ival_sub(ozni_ival a, ozni_ival b, ozni_ival *out);
[[nodiscard]] ozni_status ozni_ival_scale(ozni_ival a, ozni_rat k, ozni_ival *out);
[[nodiscard]] ozni_status ozni_ival_div(ozni_ival a, ozni_rat k, ozni_ival *out);
bool ozni_ival_is_point(ozni_ival a);

#endif
