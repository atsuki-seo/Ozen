/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_ARENA_H
#define OZNI_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ozni.h"

typedef uint32_t ozni_index;
static_assert(sizeof(ozni_index) == 4);

typedef struct ozni_arena ozni_arena;
typedef struct ozni_arena_table ozni_table;

[[nodiscard]] ozni_arena *ozni_arena_new(const ozni_allocator *a);
void ozni_arena_free(ozni_arena *ar);
[[nodiscard]] void *ozni_arena_alloc(ozni_arena *ar, size_t size);
uint32_t ozni_arena_failures(const ozni_arena *ar);

[[nodiscard]] ozni_table *ozni_table_new(ozni_arena *ar, size_t elem_size);
[[nodiscard]] bool ozni_table_push(ozni_table *t, const void *elem, ozni_index *out);
void *ozni_table_at(ozni_table *t, ozni_index i);
const void *ozni_table_get(const ozni_table *t, ozni_index i);
ozni_index ozni_table_count(const ozni_table *t);

#endif
