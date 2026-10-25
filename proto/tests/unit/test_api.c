/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

void test_limits_defaults(void) {
  ozp_limits l = ozp_default_limits();
  CHECK(l.steps == 1000000);
  CHECK(l.depth == 256);
  CHECK(l.nodes == 100000);
  CHECK(l.input_bytes == 1048576);
}

static char *slurp(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (f == nullptr) {
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)n + 1);
  if (buf != nullptr && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    buf = nullptr;
  }
  fclose(f);
  *len = (size_t)n;
  return buf;
}

void test_alloc_balance(void) {
  CHECK(ozp_test_argc > 0);
  for (int i = 0; i < ozp_test_argc; i++) {
    size_t len = 0;
    char *src = slurp(ozp_test_argv[i], &len);
    CHECK(src != nullptr);
    if (src == nullptr) {
      continue;
    }
    test_alloc t = {0};
    ozp_allocator a = test_allocator(&t);
    ozp_options opt = {.allocator = &a, .limits = ozp_default_limits()};
    char *out = ozp_expand(src, len, &opt);
    CHECK(out != nullptr);
    ozp_free_string(&opt, out);
    if (t.live != 0) {
      fprintf(stderr, "%s: %ld blocks not released\n", ozp_test_argv[i], t.live);
    }
    CHECK(t.live == 0);
    free(src);
  }
}
