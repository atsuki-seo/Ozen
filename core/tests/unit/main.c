/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

int ozn_test_failures = 0;
int ozn_test_argc = 0;
char **ozn_test_argv = nullptr;

static void *test_alloc_fn(void *ctx, size_t size) {
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

static void test_release_fn(void *ctx, void *ptr, size_t size) {
  test_alloc *t = ctx;
  (void)size;
  if (ptr != nullptr) {
    t->live--;
    free(ptr);
  }
}

ozni_allocator test_allocator(test_alloc *t) { return (ozni_allocator){test_alloc_fn, test_release_fn, t}; }

#define OZN_TEST(name) void test_##name(void);
#include "tests.def"
#undef OZN_TEST

static const struct {
  const char *name;
  void (*fn)(void);
} tests[] = {
#define OZN_TEST(name) {#name, test_##name},
#include "tests.def"
#undef OZN_TEST
};

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s TEST\n", argv[0]);
    return 2;
  }
  for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
    if (strcmp(tests[i].name, argv[1]) == 0) {
      ozn_test_argc = argc - 2;
      ozn_test_argv = argv + 2;
      tests[i].fn();
      return ozn_test_failures == 0 ? 0 : 1;
    }
  }
  fprintf(stderr, "unknown test %s\n", argv[1]);
  return 2;
}
