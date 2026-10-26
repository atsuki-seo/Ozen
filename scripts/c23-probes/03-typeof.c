/* SPDX-License-Identifier: MIT OR Apache-2.0 */
int f(void);
int f(void) {
  const int a = 1;
  typeof(a) b = a;
  typeof_unqual(a) c = b;
  c++;
  return c;
}
