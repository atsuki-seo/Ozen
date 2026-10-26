/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZEN_H
#define OZEN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct recipe_source {
  const char *path;
  const char *text;
  size_t len;
} recipe_source;

enum {
  RECIPE_EMIT_IR = 1,
  RECIPE_EMIT_SCHEDULE = 2,
  RECIPE_EMIT_STEPS = 4,
  RECIPE_EMIT_SIGNATURES = 8,
  RECIPE_EMIT_DEFINITIONS = 16,
  RECIPE_EMIT_TYPES = 32
};

typedef struct recipe_config {
  const char *entry;
  unsigned emit;
  unsigned long max_steps;
  unsigned long max_depth;
  unsigned long max_nodes;
  unsigned long max_input_bytes;
  void *(*alloc)(void *ctx, size_t size);
  void (*release)(void *ctx, void *ptr, size_t size);
  void *alloc_ctx;
} recipe_config;

recipe_config recipe_default_config(void);
char *recipe_build(const recipe_config *cfg, const recipe_source *sources, size_t n_sources, const char *kitchen,
                   size_t kitchen_len, const char *options, size_t options_len);
void recipe_free_string(const recipe_config *cfg, char *s);

#ifdef __cplusplus
}
#endif

#endif
