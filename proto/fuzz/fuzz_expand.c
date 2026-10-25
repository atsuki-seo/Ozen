/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdint.h>
#include <stdlib.h>

#include "ozenproto.h"

static void *fz_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void fz_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ozp_allocator a = {fz_alloc, fz_release, nullptr};
  ozp_options opt = {.allocator = &a,
                     .limits = {.steps = 20000, .depth = 64, .nodes = 2000, .input_bytes = 65536},
                     .stats = 1};
  char *out = ozp_expand((const char *)data, size, &opt);
  ozp_free_string(&opt, out);
  return 0;
}
