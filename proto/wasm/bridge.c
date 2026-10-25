/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdlib.h>

#include "ozenproto.h"

static void *br_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void br_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static const ozp_allocator allocator = {br_alloc, br_release, nullptr};

char *ozpw_expand(const char *src, size_t len);
void ozpw_free(char *s);

char *ozpw_expand(const char *src, size_t len) {
  ozp_options opt = {.allocator = &allocator, .limits = ozp_default_limits()};
  return ozp_expand(src, len, &opt);
}

void ozpw_free(char *s) {
  ozp_options opt = {.allocator = &allocator, .limits = ozp_default_limits()};
  ozp_free_string(&opt, s);
}
