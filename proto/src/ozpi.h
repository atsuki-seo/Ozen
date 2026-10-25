/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZPI_H
#define OZPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ozenproto.h"

typedef struct ozpi_chunk ozpi_chunk;

typedef struct ozpi_arena {
  const ozp_allocator *allocator;
  ozpi_chunk *head;
  bool failed;
} ozpi_arena;

void ozpi_arena_init(ozpi_arena *ar, const ozp_allocator *allocator);
[[nodiscard]] void *ozpi_arena_alloc(ozpi_arena *ar, size_t size);
void ozpi_arena_free_all(ozpi_arena *ar);

typedef enum ozpi_diag_id : uint8_t {
#define OZPI_DIAG(code, cat, name) OZPI_D_##cat##_##name,
#include "diag_codes.def"
#undef OZPI_DIAG
  OZPI_D_COUNT
} ozpi_diag_id;

const char *ozpi_diag_code(ozpi_diag_id id);
const char *ozpi_diag_name(ozpi_diag_id id);

#endif
