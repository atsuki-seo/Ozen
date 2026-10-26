/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_RESOLVE_H
#define OZNI_RESOLVE_H

#include "ast.h"
#include "buf.h"

typedef struct ozni_def {
  ozni_index name;
  ozni_index node;
  bool local;
} ozni_def;

typedef struct ozni_module {
  ozni_index name;
  ozni_index file;
  ozni_index pkg;
  ozni_index root;
  bool is_std;
  bool has_recipe;
  bool has_menu;
  ozni_table *defs;
  ozni_table *bindings;
  ozni_index *slots;
  uint32_t cap;
} ozni_module;

typedef struct ozni_package {
  ozni_index name;
  ozni_index version;
  ozni_index file;
  ozni_index decl;
  const char *prefix;
  size_t prefix_len;
  bool is_std;
} ozni_package;

typedef struct ozni_project {
  ozni_ctx *c;
  size_t nuser;
  ozni_ast *asts;
  ozni_table *modules;
  ozni_table *packages;
  ozni_table *values;
  ozni_index root_pkg;
  ozni_index std_pkg;
  ozni_index prelude;
  bool syntax_errors;
} ozni_project;

[[nodiscard]] bool ozni_project_load(ozni_ctx *c, const ozni_source *user, size_t nuser, ozni_project *pr);
const ozni_module *ozni_project_module(const ozni_project *pr, ozni_index i);
bool ozni_render_modules(const ozni_project *pr, ozni_buf *b);
bool ozni_project_find(const ozni_project *pr, ozni_index mod, ozni_index name, ozni_index *out_mod,
                       ozni_index *out_def);
ozni_index ozni_project_find_module(const ozni_project *pr, ozni_index name);
const ozni_node *ozni_project_node(const ozni_project *pr, ozni_index file, ozni_index i);
bool ozni_render_definitions(const ozni_project *pr, ozni_buf *b);
bool ozni_render_public(const ozni_project *pr, ozni_buf *b, unsigned emit);
bool ozni_render_entry(ozni_project *pr, const char *entry, ozni_buf *b, bool *ok);
[[nodiscard]] bool ozni_apply_levels(ozni_project *pr);

#endif
