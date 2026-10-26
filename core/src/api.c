/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "../include/ozen.h"
#include "ozni.h"

recipe_config recipe_default_config(void) { return (recipe_config){0}; }

static uint64_t or_default(unsigned long v, uint64_t def) { return v != 0 ? (uint64_t)v : def; }

char *recipe_build(const recipe_config *cfg, const recipe_source *sources, size_t n_sources, const char *kitchen,
                   size_t kitchen_len, const char *options, size_t options_len) {
  if (cfg == NULL || cfg->alloc == NULL || cfg->release == NULL || (sources == NULL && n_sources > 0)) {
    return NULL;
  }
  ozni_allocator a = {cfg->alloc, cfg->release, cfg->alloc_ctx};
  ozni_limits def = ozni_default_limits();
  ozni_limits lim = {or_default(cfg->max_steps, def.steps), (uint32_t)or_default(cfg->max_depth, def.depth),
                     (uint32_t)or_default(cfg->max_nodes, def.nodes),
                     (size_t)or_default(cfg->max_input_bytes, def.input_bytes)};
  size_t total = n_sources + (kitchen != NULL ? 1 : 0) + (options != NULL ? 1 : 0);
  ozni_source *srcs = a.alloc(a.ctx, (total + 1) * sizeof *srcs);
  if (srcs == NULL) {
    return NULL;
  }
  for (size_t k = 0; k < n_sources; k++) {
    srcs[k] = (ozni_source){sources[k].path, sources[k].text, sources[k].len};
  }
  size_t at = n_sources;
  if (kitchen != NULL) {
    srcs[at++] = (ozni_source){"<kitchen>", kitchen, kitchen_len};
  }
  if (options != NULL) {
    srcs[at++] = (ozni_source){"<options>", options, options_len};
  }
  ozni_settings set = {.kitchen = kitchen != NULL ? "<kitchen>" : NULL,
                       .options = options != NULL ? "<options>" : NULL,
                       .entry = cfg->entry};
  int has_errors = 0;
  char *out = ozni_run_json(&a, &lim, &set, srcs, total, cfg->emit, &has_errors);
  a.release(a.ctx, srcs, (total + 1) * sizeof *srcs);
  return out;
}

void recipe_free_string(const recipe_config *cfg, char *s) {
  if (cfg != NULL && cfg->release != NULL && s != NULL) {
    cfg->release(cfg->alloc_ctx, s, strlen(s) + 1);
  }
}
