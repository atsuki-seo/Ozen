/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdint.h>
#include <stdlib.h>

#include "ozni.h"

static void *heap_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void heap_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ozni_allocator a = {heap_alloc, heap_release, nullptr};
  ozni_limits lim = ozni_default_limits();
  lim.steps = 20000;
  ozni_settings set = {0};
  ozni_source src = {"shared.ozen", (const char *)data, size};
  int has_errors = 0;
  ozni_free_text(&a, ozni_run_text(&a, &lim, &set, &src, 1, OZNI_MODE_CHECK, nullptr, &has_errors));
  ozni_free_text(&a, ozni_unbundle_text(&a, &lim, &src, &has_errors));
  ozni_source pkg[2] = {{"package.ozen", "package book {\n  version: \"1.0.0\"\n}\n", 36}, src};
  ozni_free_text(&a, ozni_bundle_text(&a, &lim, &set, pkg, 2, nullptr, 0, &has_errors));
  return 0;
}
