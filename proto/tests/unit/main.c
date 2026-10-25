/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

int ozp_test_failures = 0;

void *test_alloc_fn(void *ctx, size_t size) {
  test_alloc *t = ctx;
  t->allocs++;
  if (t->fail_at != 0 && t->allocs == t->fail_at) {
    return nullptr;
  }
  void *p = malloc(size);
  if (p != nullptr) {
    t->live++;
  }
  return p;
}

void test_release_fn(void *ctx, void *ptr, size_t size) {
  test_alloc *t = ctx;
  (void)size;
  if (ptr != nullptr) {
    t->live--;
    free(ptr);
  }
}

ozp_allocator test_allocator(test_alloc *t) { return (ozp_allocator){test_alloc_fn, test_release_fn, t}; }

#define OZP_TEST(name) void test_##name(void);
#include "tests.def"
#undef OZP_TEST

static const struct {
  const char *name;
  void (*fn)(void);
} tests[] = {
#define OZP_TEST(name) {#name, test_##name},
#include "tests.def"
#undef OZP_TEST
};

int main(int argc, char **argv) {
  if (argc != 2) {
    fputs("usage: ozp_unit TEST\n", stderr);
    return 2;
  }
  for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
    if (strcmp(tests[i].name, argv[1]) == 0) {
      tests[i].fn();
      return ozp_test_failures == 0 ? 0 : 1;
    }
  }
  fprintf(stderr, "unknown test %s\n", argv[1]);
  return 2;
}
