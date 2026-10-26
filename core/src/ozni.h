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
} ozni_settings;

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

ozni_limits ozni_default_limits(void);

enum { OZNI_AST_SPANS = 1, OZNI_AST_RULES = 2 };

typedef enum ozni_mode : uint8_t {
  OZNI_MODE_CHECK,
  OZNI_MODE_MODULES,
  OZNI_MODE_ENTRY,
  OZNI_MODE_EMBEDDED_STD,
  OZNI_MODE_DEFINITIONS,
  OZNI_MODE_STEPS,
} ozni_mode;

char *ozni_run_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                    const ozni_source *srcs, size_t n, ozni_mode mode, const char *entry, int *has_errors);
char *ozni_debug_ast_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *src,
                          unsigned flags, int *has_errors);
void ozni_free_text(const ozni_allocator *a, char *text);

#endif
