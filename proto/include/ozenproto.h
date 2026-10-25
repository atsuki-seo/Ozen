/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZENPROTO_H
#define OZENPROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ozp_allocator {
  void *(*alloc)(void *ctx, size_t size);
  void (*release)(void *ctx, void *ptr, size_t size);
  void *ctx;
} ozp_allocator;

typedef struct ozp_limits {
  uint32_t steps, depth, nodes, input_bytes;
} ozp_limits;

typedef struct ozp_options {
  const ozp_allocator *allocator;
  ozp_limits limits;
  const char *entry;
  const char *params;
  int stats;
} ozp_options;

ozp_limits ozp_default_limits(void);
char *ozp_expand(const char *src, size_t src_len, const ozp_options *opt);
void ozp_free_string(const ozp_options *opt, char *s);

#ifdef __cplusplus
}
#endif

#endif
