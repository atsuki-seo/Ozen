/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZPI_NUM_H
#define OZPI_NUM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ozpi_u128 {
  uint64_t hi, lo;
} ozpi_u128;

typedef struct ozpi_i128 {
  bool neg;
  ozpi_u128 mag;
} ozpi_i128;

ozpi_u128 ozpi_u128_from(uint64_t v);
ozpi_u128 ozpi_u128_add(ozpi_u128 a, ozpi_u128 b);
ozpi_u128 ozpi_u128_sub(ozpi_u128 a, ozpi_u128 b);
ozpi_u128 ozpi_u128_mul64(uint64_t a, uint64_t b);
int ozpi_u128_cmp(ozpi_u128 a, ozpi_u128 b);
bool ozpi_u128_is_zero(ozpi_u128 a);
void ozpi_u128_divmod(ozpi_u128 n, ozpi_u128 d, ozpi_u128 *q, ozpi_u128 *r);
ozpi_u128 ozpi_u128_gcd(ozpi_u128 a, ozpi_u128 b);

typedef struct ozpi_rat {
  int64_t num;
  int64_t den;
} ozpi_rat;

typedef enum ozpi_status : uint8_t { OZPI_OK, OZPI_OVERFLOW, OZPI_DIVZERO } ozpi_status;

constexpr int64_t OZPI_RAT_MAX = INT64_MAX;

ozpi_rat ozpi_rat_int(int64_t v);
[[nodiscard]] ozpi_status ozpi_rat_from_i128(ozpi_i128 n, ozpi_u128 d, ozpi_rat *out);
[[nodiscard]] ozpi_status ozpi_rat_make(int64_t n, int64_t d, ozpi_rat *out);
[[nodiscard]] ozpi_status ozpi_rat_add(ozpi_rat a, ozpi_rat b, ozpi_rat *out);
[[nodiscard]] ozpi_status ozpi_rat_sub(ozpi_rat a, ozpi_rat b, ozpi_rat *out);
[[nodiscard]] ozpi_status ozpi_rat_mul(ozpi_rat a, ozpi_rat b, ozpi_rat *out);
[[nodiscard]] ozpi_status ozpi_rat_div(ozpi_rat a, ozpi_rat b, ozpi_rat *out);
ozpi_rat ozpi_rat_neg(ozpi_rat a);
int ozpi_rat_cmp(ozpi_rat a, ozpi_rat b);
ozpi_rat ozpi_rat_floor(ozpi_rat a);
ozpi_rat ozpi_rat_ceil(ozpi_rat a);
bool ozpi_rat_is_int(ozpi_rat a);

typedef struct ozpi_ival {
  ozpi_rat lo, hi;
} ozpi_ival;

[[nodiscard]] ozpi_status ozpi_ival_add(ozpi_ival a, ozpi_ival b, ozpi_ival *out);
[[nodiscard]] ozpi_status ozpi_ival_sub(ozpi_ival a, ozpi_ival b, ozpi_ival *out);
[[nodiscard]] ozpi_status ozpi_ival_scale(ozpi_ival a, ozpi_rat k, ozpi_ival *out);
[[nodiscard]] ozpi_status ozpi_ival_div(ozpi_ival a, ozpi_rat k, ozpi_ival *out);
bool ozpi_ival_is_point(ozpi_ival a);

#endif
