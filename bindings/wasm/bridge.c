/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdlib.h>

#include "ozen.h"

static void *bridge_alloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void bridge_release(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)size;
  free(ptr);
}

static recipe_config bridge_config(const char *entry, unsigned emit) {
  recipe_config cfg = recipe_default_config();
  cfg.alloc = bridge_alloc;
  cfg.release = bridge_release;
  cfg.entry = entry;
  cfg.emit = emit;
  return cfg;
}

char *ozw_build(const char *const *paths, const char *const *texts, const size_t *lens, size_t n, const char *kitchen,
                size_t kitchen_len, const char *options, size_t options_len, const char *entry, unsigned emit);
void ozw_free(char *s);

char *ozw_build(const char *const *paths, const char *const *texts, const size_t *lens, size_t n, const char *kitchen,
                size_t kitchen_len, const char *options, size_t options_len, const char *entry, unsigned emit) {
  recipe_source *srcs = calloc(n + 1, sizeof *srcs);
  if (srcs == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < n; i++) {
    srcs[i] = (recipe_source){paths[i], texts[i], lens[i]};
  }
  recipe_config cfg = bridge_config(entry, emit);
  char *out = recipe_build(&cfg, srcs, n, kitchen, kitchen_len, options, options_len);
  free(srcs);
  return out;
}

void ozw_free(char *s) {
  recipe_config cfg = bridge_config(NULL, 0);
  recipe_free_string(&cfg, s);
}
