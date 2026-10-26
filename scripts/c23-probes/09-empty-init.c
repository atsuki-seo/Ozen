/* SPDX-License-Identifier: MIT OR Apache-2.0 */
struct pair {
  int a, b;
};
int f(void);
int f(void) {
  struct pair p = {};
  return p.a + p.b;
}
