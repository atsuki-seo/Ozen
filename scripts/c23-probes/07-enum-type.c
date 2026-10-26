/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdint.h>
enum small : uint8_t { small_a, small_b };
int f(void);
int f(void) { return (int)sizeof(enum small); }
