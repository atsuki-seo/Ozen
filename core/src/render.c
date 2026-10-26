/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "render.h"
#include "text.h"

typedef struct pos {
  uint32_t line;
  uint32_t col;
} pos;

static pos pos_of(const ozni_ctx *c, ozni_index file, uint32_t offset) {
  pos r = {1, 1};
  if (file >= c->nsrcs) {
    return r;
  }
  const ozni_source *s = &c->srcs[file];
  for (uint32_t i = 0; i < offset && i < s->len; i++) {
    if (s->text[i] == '\n') {
      r.line++;
      r.col = 1;
    } else {
      r.col++;
    }
  }
  return r;
}

static void put_span(ozni_buf *b, const ozni_ctx *c, ozni_span sp) {
  pos a = pos_of(c, sp.file, sp.start), e = pos_of(c, sp.file, sp.end);
  ozni_buf_putu(b, a.line);
  ozni_buf_puts(b, ":");
  ozni_buf_putu(b, a.col);
  ozni_buf_puts(b, "-");
  ozni_buf_putu(b, e.line);
  ozni_buf_puts(b, ":");
  ozni_buf_putu(b, e.col);
}

static void put_path(ozni_buf *b, const ozni_ctx *c, ozni_index file) {
  ozni_buf_puts(b, file < c->nsrcs ? c->srcs[file].path : "?");
}

static void put_english(ozni_buf *b, const char *name) {
  for (const char *s = name; *s != '\0'; s++) {
    if (*s >= 'A' && *s <= 'Z') {
      char ch = (char)(*s - 'A' + 'a');
      ozni_buf_put(b, " ", 1);
      ozni_buf_put(b, &ch, 1);
    } else {
      ozni_buf_put(b, s, 1);
    }
  }
}

static int cmp_str(ozni_str a, ozni_str b) {
  size_t n = a.len < b.len ? a.len : b.len;
  int r = n == 0 ? 0 : memcmp(a.p, b.p, n);
  if (r != 0) {
    return r;
  }
  return a.len < b.len ? -1 : a.len > b.len ? 1 : 0;
}

static int cmp_diag(const ozni_ctx *c, const ozni_diag *x, const ozni_diag *y) {
  if (x->span.file != y->span.file) {
    return x->span.file < y->span.file ? -1 : 1;
  }
  if (x->span.start != y->span.start) {
    return x->span.start < y->span.start ? -1 : 1;
  }
  int r = ozni_strcmp(ozni_diag_code(x->id), ozni_diag_code(y->id));
  if (r != 0) {
    return r;
  }
  for (uint16_t i = 0; i < x->nargs && i < y->nargs; i++) {
    r = cmp_str(ozni_diag_arg(c, x, i), ozni_diag_arg(c, y, i));
    if (r != 0) {
      return r;
    }
  }
  return x->nargs < y->nargs ? -1 : x->nargs > y->nargs ? 1 : 0;
}

static void sort_diags(const ozni_ctx *c, const ozni_diag **v, const ozni_diag **tmp, size_t n) {
  if (n < 2) {
    return;
  }
  size_t h = n / 2;
  sort_diags(c, v, tmp, h);
  sort_diags(c, v + h, tmp, n - h);
  size_t i = 0, j = h, k = 0;
  while (i < h || j < n) {
    if (j >= n || (i < h && cmp_diag(c, v[i], v[j]) <= 0)) {
      tmp[k++] = v[i++];
    } else {
      tmp[k++] = v[j++];
    }
  }
  memcpy(v, tmp, n * sizeof *v);
}

static const char *severity_word(ozni_severity s) {
  switch (s) {
  case OZNI_SEV_ERROR:
    return "error";
  case OZNI_SEV_WARNING:
    return "warning";
  case OZNI_SEV_INFO:
    return "info";
  }
  return "error";
}

bool ozni_render_check(ozni_ctx *c, ozni_buf *b, bool *has_errors) {
  ozni_index n = ozni_table_count(c->diags);
  const ozni_diag **v = ozni_arena_alloc(c->arena, (n + 1) * sizeof *v * 2);
  if (v == nullptr) {
    return false;
  }
  for (ozni_index i = 0; i < n; i++) {
    v[i] = ozni_table_get(c->diags, i);
  }
  sort_diags(c, v, v + n, n);
  uint64_t counts[3] = {0, 0, 0};
  for (ozni_index i = 0; i < n; i++) {
    const ozni_diag *d = v[i];
    counts[d->severity]++;
    ozni_buf_puts(b, severity_word(d->severity));
    ozni_buf_puts(b, " ");
    ozni_buf_puts(b, ozni_diag_code(d->id));
    ozni_buf_puts(b, " ");
    ozni_buf_puts(b, ozni_diag_name(d->id));
    ozni_buf_puts(b, " ");
    put_path(b, c, d->span.file);
    ozni_buf_puts(b, ":");
    put_span(b, c, d->span);
    if (d->nargs > 0) {
      ozni_buf_puts(b, " args=[");
      for (uint16_t k = 0; k < d->nargs; k++) {
        ozni_str s = ozni_diag_arg(c, d, k);
        if (k > 0) {
          ozni_buf_puts(b, ", ");
        }
        ozni_buf_quoted(b, s.p, s.len);
      }
      ozni_buf_puts(b, "]");
    }
    ozni_buf_puts(b, ": ");
    put_english(b, ozni_diag_name(d->id));
    ozni_buf_puts(b, "\n");
  }
  *has_errors = counts[OZNI_SEV_ERROR] > 0;
  ozni_buf_puts(b, *has_errors ? "result error errors=" : "result ok errors=");
  ozni_buf_putu(b, counts[OZNI_SEV_ERROR]);
  ozni_buf_puts(b, " warnings=");
  ozni_buf_putu(b, counts[OZNI_SEV_WARNING]);
  ozni_buf_puts(b, " infos=");
  ozni_buf_putu(b, counts[OZNI_SEV_INFO]);
  ozni_buf_puts(b, "\n");
  return !b->failed;
}

static void put_text(ozni_buf *b, const ozni_ctx *c, ozni_index text) {
  ozni_str s = ozni_intern_get(c->names, text);
  ozni_buf_put(b, s.p, s.len);
}

static void dump(const ozni_ctx *c, const ozni_ast *ast, ozni_index i, unsigned depth, unsigned flags, ozni_buf *b) {
  const ozni_node *n = ozni_node_get(ast, i);
  if (n == nullptr) {
    return;
  }
  for (unsigned k = 0; k < depth; k++) {
    ozni_buf_puts(b, "  ");
  }
  ozni_buf_puts(b, ozni_node_kind_name(n->kind));
  ozni_index child = n->first;
  if (n->kind == OZNI_N_String || n->kind == OZNI_N_MlString) {
    ozni_str s = ozni_intern_get(c->names, n->text);
    ozni_buf_puts(b, " ");
    ozni_buf_quoted(b, s.p, s.len);
  } else if (n->kind == OZNI_N_Check) {
    if (n->text2 != 0) {
      ozni_buf_puts(b, " ");
      put_text(b, c, n->text2);
    }
    ozni_buf_puts(b, " ");
    put_text(b, c, n->text);
    if (n->text3 != 0) {
      ozni_buf_puts(b, " else ");
      put_text(b, c, n->text3);
    }
  } else if (n->kind == OZNI_N_BundleFrom) {
    ozni_buf_puts(b, " ");
    put_text(b, c, n->text);
    ozni_str s = ozni_intern_get(c->names, n->text2);
    ozni_buf_puts(b, " ");
    ozni_buf_quoted(b, s.p, s.len);
  } else if (n->kind == OZNI_N_LetTuple) {
    while (child != 0 && ozni_node_get(ast, child)->kind == OZNI_N_BindName) {
      ozni_buf_puts(b, " ");
      put_text(b, c, ozni_node_get(ast, child)->text);
      child = ozni_node_get(ast, child)->next;
    }
  } else if (n->kind == OZNI_N_Quantity) {
    while (child != 0 && ozni_node_get(ast, child)->kind == OZNI_N_QtyPart) {
      const ozni_node *q = ozni_node_get(ast, child);
      ozni_buf_puts(b, " ");
      put_text(b, c, q->text);
      ozni_buf_puts(b, " ");
      put_text(b, c, q->text2);
      child = q->next;
    }
  } else {
    if (n->text != 0) {
      ozni_buf_puts(b, " ");
      put_text(b, c, n->text);
    }
    if (n->text2 != 0) {
      ozni_buf_puts(b, " ");
      put_text(b, c, n->text2);
    }
  }
  if ((flags & OZNI_AST_SPANS) != 0 && n->kind != OZNI_N_File) {
    ozni_buf_puts(b, " ");
    put_span(b, c, n->span);
  }
  ozni_buf_puts(b, "\n");
  for (; child != 0; child = ozni_node_get(ast, child)->next) {
    dump(c, ast, child, depth + 1, flags, b);
  }
}

bool ozni_render_ast(const ozni_ctx *c, const ozni_ast *ast, unsigned flags, ozni_buf *b) {
  if ((flags & OZNI_AST_RULES) != 0) {
    const char *names[OZNI_R_COUNT];
    size_t n = 0;
    for (unsigned r = 0; r < OZNI_R_COUNT; r++) {
      if ((ast->rules[r / 8] & (1u << (r % 8))) != 0) {
        names[n++] = ozni_rule_name((ozni_rule)r);
      }
    }
    for (size_t i = 1; i < n; i++) {
      const char *x = names[i];
      size_t j = i;
      while (j > 0 && ozni_strcmp(names[j - 1], x) > 0) {
        names[j] = names[j - 1];
        j--;
      }
      names[j] = x;
    }
    for (size_t i = 0; i < n; i++) {
      ozni_buf_puts(b, names[i]);
      ozni_buf_puts(b, "\n");
    }
    return !b->failed;
  }
  dump(c, ast, ast->root, 0, flags, b);
  return !b->failed;
}
