/* SPDX-License-Identifier: MIT OR Apache-2.0 */
[[nodiscard]] int f(void);
int f(void) {
  [[maybe_unused]] int unused = 0;
  return 1;
}
