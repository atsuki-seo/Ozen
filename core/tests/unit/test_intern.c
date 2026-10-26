/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"
#include "ctx.h"
#include "text.h"

void test_intern_same_id(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_arena *ar = ozni_arena_new(&a);
  ozni_intern *in = ozni_intern_new(ar);
  ozni_index x = 0, y = 0, z = 0;
  CHECK(ozni_intern_put(in, "onion", 5, &x));
  CHECK(ozni_intern_put(in, "carrot", 6, &y));
  CHECK(ozni_intern_put(in, "onionX", 5, &z));
  CHECK(x != 0 && x != y && z == x);
  char buf[16];
  for (int i = 0; i < 2000; i++) {
    int n = snprintf(buf, sizeof buf, "name%d", i);
    ozni_index id = 0;
    CHECK(ozni_intern_put(in, buf, (size_t)n, &id));
  }
  ozni_index again = 0;
  CHECK(ozni_intern_put(in, "onion", 5, &again) && again == x);
  ozni_str s = ozni_intern_get(in, y);
  CHECK(s.len == 6 && memcmp(s.p, "carrot", 6) == 0);
  CHECK(ozni_intern_count(in) == 2002);
  ozni_arena_free(ar);
  CHECK(t.live == 0);
}

void test_utf8_check(void) {
  size_t bad = 0;
  const char ok[] = "a\xe3\x81\x82\xf0\x9f\x8d\x9b";
  CHECK(ozni_check_utf8(ok, sizeof ok - 1, &bad));
  const char overlong[] = "ab\xc0\xaf";
  CHECK(!ozni_check_utf8(overlong, sizeof overlong - 1, &bad) && bad == 2);
  const char surrogate[] = "\xed\xa0\x80";
  CHECK(!ozni_check_utf8(surrogate, sizeof surrogate - 1, &bad) && bad == 0);
  const char truncated[] = "x\xe3\x81";
  CHECK(!ozni_check_utf8(truncated, sizeof truncated - 1, &bad) && bad == 1);
}

void test_diag_args(void) {
  test_alloc t = {0};
  ozni_allocator a = test_allocator(&t);
  ozni_limits lim = ozni_default_limits();
  ozni_ctx c;
  CHECK(ozni_ctx_init(&c, &a, &lim, nullptr, 0));
  ozni_str args[] = {{"x", 1}, {"Mass", 4}};
  CHECK(ozni_diag_add(&c, OZNI_D_NAM_rebind, (ozni_span){0, 3, 7}, 0, args, 2));
  const ozni_diag *d = ozni_table_get(c.diags, 0);
  CHECK(d != nullptr && d->id == OZNI_D_NAM_rebind && d->nargs == 2 && d->severity == OZNI_SEV_ERROR);
  if (d != nullptr) {
    ozni_str s = ozni_diag_arg(&c, d, 1);
    CHECK(s.len == 4 && memcmp(s.p, "Mass", 4) == 0);
  }
  ozni_ctx_free(&c);
  CHECK(t.live == 0);
}
