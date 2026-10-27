/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static const char recipe[] = "recipe soup() {\n  ingredients {\n    water: 400 ml\n    onion: 1 pcs\n  }\n"
                             "  let c = cut(peel(onion), slice, 3 mm)\n  let s = simmer([water, c], 10 min)\n}\n"
                             "menu lunch(people: 2) {\n  let s = make soup()\n  serve main {\n"
                             "    plate perPerson on SoupBowl { s }\n  }\n}\n";

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ozni_allocator a = {heap_alloc, heap_release, nullptr};
  ozni_limits lim = ozni_default_limits();
  lim.steps = 20000;
  ozni_settings set = {.kitchen = "<kitchen>", .entry = "lunch"};
  ozni_source srcs[2] = {{"lunch.ozen", recipe, strlen(recipe)}, {"<kitchen>", (const char *)data, size}};
  int has_errors = 0;
  ozni_free_text(&a, ozni_run_text(&a, &lim, &set, srcs, 2, OZNI_MODE_SCHEDULE, "lunch", &has_errors));
  return 0;
}
