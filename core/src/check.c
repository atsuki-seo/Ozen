/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "ozni.h"

ozni_limits ozni_default_limits(void) {
  return (ozni_limits){.steps = 1000000, .depth = 256, .nodes = 100000, .input_bytes = 1048576};
}

static char *copy_text(const ozni_allocator *a, const char *s) {
  size_t n = strlen(s);
  char *out = a->alloc(a->ctx, n + 1);
  if (out != nullptr) {
    memcpy(out, s, n + 1);
  }
  return out;
}

char *ozni_check_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *srcs, size_t n,
                      int *has_errors) {
  (void)lim;
  (void)srcs;
  (void)n;
  *has_errors = 0;
  return copy_text(a, "result ok errors=0 warnings=0 infos=0\n");
}

char *ozni_debug_ast_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *src,
                          unsigned flags, int *has_errors) {
  (void)lim;
  (void)src;
  (void)flags;
  *has_errors = 0;
  return copy_text(a, "");
}

void ozni_free_text(const ozni_allocator *a, char *text) {
  if (text != nullptr) {
    a->release(a->ctx, text, strlen(text) + 1);
  }
}
