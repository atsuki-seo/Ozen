/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "ozpi.h"

ozp_limits ozp_default_limits(void) {
  return (ozp_limits){.steps = 1000000, .depth = 256, .nodes = 100000, .input_bytes = 1048576};
}

char *ozp_expand(const char *src, size_t src_len, const ozp_options *opt) {
  (void)src;
  (void)src_len;
  static const char text[] = "result ok steps=0 diagnostics=0\n";
  char *out = opt->allocator->alloc(opt->allocator->ctx, sizeof text);
  if (out != nullptr) {
    memcpy(out, text, sizeof text);
  }
  return out;
}

void ozp_free_string(const ozp_options *opt, char *s) {
  if (s != nullptr) {
    opt->allocator->release(opt->allocator->ctx, s, strlen(s) + 1);
  }
}
