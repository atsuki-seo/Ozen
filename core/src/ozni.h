/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_H
#define OZNI_H

#include <stddef.h>
#include <stdint.h>

typedef struct ozni_allocator {
  void *(*alloc)(void *ctx, size_t size);
  void (*release)(void *ctx, void *ptr, size_t size);
  void *ctx;
} ozni_allocator;

typedef struct ozni_limits {
  uint64_t steps;
  uint32_t depth;
  uint32_t nodes;
  size_t input_bytes;
} ozni_limits;

typedef struct ozni_settings {
  bool strict;
  const char *kitchen;
  const char *options;
  const char *entry;
  const char *const *level_names;
  const uint8_t *level_kinds;
  size_t nlevels;
  bool fixes;
  bool json;
  bool api;
} ozni_settings;

enum { OZNI_LEVEL_ALLOW, OZNI_LEVEL_WARN, OZNI_LEVEL_DENY, OZNI_LEVEL_EXPECT };

typedef struct ozni_source {
  const char *path;
  const char *text;
  size_t len;
} ozni_source;

typedef enum ozni_severity : uint8_t {
  OZNI_SEV_ERROR,
  OZNI_SEV_WARNING,
  OZNI_SEV_INFO,
} ozni_severity;

typedef enum ozni_diag_id : uint16_t {
#define OZNI_DIAG(code, cat, name, sev) OZNI_D_##cat##_##name,
#include "diag_codes.def"
#undef OZNI_DIAG
  OZNI_D_COUNT
} ozni_diag_id;

const char *ozni_diag_code(ozni_diag_id id);
const char *ozni_diag_name(ozni_diag_id id);
ozni_severity ozni_diag_default_severity(ozni_diag_id id);
bool ozni_diag_lookup(const char *code, size_t len, ozni_diag_id *out);

ozni_limits ozni_default_limits(void);

enum { OZNI_AST_SPANS = 1, OZNI_AST_RULES = 2 };

typedef enum ozni_mode : uint8_t {
  OZNI_MODE_CHECK,
  OZNI_MODE_MODULES,
  OZNI_MODE_ENTRY,
  OZNI_MODE_EMBEDDED_STD,
  OZNI_MODE_DEFINITIONS,
  OZNI_MODE_STEPS,
  OZNI_MODE_REQUIRES,
  OZNI_MODE_IR,
  OZNI_MODE_SCHEDULE,
} ozni_mode;

char *ozni_run_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                    const ozni_source *srcs, size_t n, ozni_mode mode, const char *entry, int *has_errors);
char *ozni_debug_ast_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *src,
                          unsigned flags, int *has_errors);
char *ozni_bundle_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                       const ozni_source *srcs, size_t n, const ozni_source *phrases, size_t nphrases, int *has_errors);
void ozni_free_text(const ozni_allocator *a, char *text);
char *ozni_run_json(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                    const ozni_source *srcs, size_t n, unsigned emit, int *has_errors);

#endif
