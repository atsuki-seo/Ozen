/* SPDX-License-Identifier: MIT OR Apache-2.0 */
int f(void);
int f(void) {
  int *p = nullptr;
  return p == nullptr ? 0 : 1;
}
