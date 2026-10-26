/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stddef.h>
int f(int x);
int f(int x) {
  if (x > 0) {
    return x;
  }
  if (x <= 0) {
    return -x;
  }
  unreachable();
}
