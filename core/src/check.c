/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "render.h"
#include "text.h"

ozni_limits ozni_default_limits(void) {
  return (ozni_limits){.steps = 1000000, .depth = 256, .nodes = 100000, .input_bytes = 1048576};
}

static void put_size(char *out, size_t *n, uint64_t v) {
  char rev[24];
  size_t k = 0;
  do {
    rev[k++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0);
  for (size_t i = 0; i < k; i++) {
    out[i] = rev[k - 1 - i];
  }
  *n = k;
}

static bool admit(ozni_ctx *c, ozni_index file) {
  const ozni_source *s = &c->srcs[file];
  if (s->len > c->lim.input_bytes) {
    char a[24], b[24];
    size_t na, nb;
    put_size(a, &na, s->len);
    put_size(b, &nb, c->lim.input_bytes);
    ozni_str args[2] = {{a, (uint32_t)na}, {b, (uint32_t)nb}};
    ozni_diag_add(c, OZNI_D_SYN_inputTooLarge, (ozni_span){file, 0, 0}, 0, args, 2);
    return false;
  }
  size_t bad = 0;
  if (!ozni_check_utf8(s->text, s->len, &bad)) {
    ozni_diag_add(c, OZNI_D_SYN_invalidUtf8, (ozni_span){file, (uint32_t)bad, (uint32_t)bad + 1}, 0, nullptr, 0);
    return false;
  }
  return true;
}

char *ozni_check_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *srcs, size_t n,
                      int *has_errors) {
  ozni_ctx c;
  *has_errors = 0;
  if (!ozni_ctx_init(&c, a, lim, srcs, n)) {
    return nullptr;
  }
  for (ozni_index f = 0; f < n && !c.oom; f++) {
    if (admit(&c, f)) {
      ozni_ast ast;
      if (!ozni_parse(&c, f, &ast)) {
        c.oom = true;
      }
    }
  }
  ozni_buf b = {.a = a};
  bool errors = false;
  char *out = nullptr;
  if (!c.oom && ozni_render_check(&c, &b, &errors)) {
    out = ozni_buf_finish(&b);
  } else {
    ozni_buf_discard(&b);
  }
  *has_errors = errors;
  ozni_ctx_free(&c);
  return out;
}

char *ozni_debug_ast_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_source *src,
                          unsigned flags, int *has_errors) {
  ozni_ctx c;
  *has_errors = 0;
  if (!ozni_ctx_init(&c, a, lim, src, 1)) {
    return nullptr;
  }
  ozni_ast ast = {0};
  bool ok = admit(&c, 0) && ozni_parse(&c, 0, &ast);
  ozni_buf b = {.a = a};
  char *out = nullptr;
  if (ok && ozni_render_ast(&c, &ast, flags, &b)) {
    out = ozni_buf_finish(&b);
  } else if (!c.oom && (flags & OZNI_AST_RULES) != 0) {
    ozni_buf_discard(&b);
    out = ozni_buf_finish(&b);
  } else if (!c.oom) {
    ozni_buf_discard(&b);
    bool errors = false;
    if (ozni_render_check(&c, &b, &errors)) {
      out = ozni_buf_finish(&b);
    }
  } else {
    ozni_buf_discard(&b);
  }
  for (ozni_index i = 0; i < ozni_table_count(c.diags); i++) {
    const ozni_diag *d = ozni_table_get(c.diags, i);
    if (d->severity == OZNI_SEV_ERROR) {
      *has_errors = 1;
    }
  }
  ozni_ctx_free(&c);
  return out;
}

void ozni_free_text(const ozni_allocator *a, char *text) {
  if (text != nullptr) {
    a->release(a->ctx, text, strlen(text) + 1);
  }
}
