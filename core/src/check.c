/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "render.h"
#include "resolve.h"
#include "sema.h"
#include "stdembed.h"
#include "text.h"

ozni_limits ozni_default_limits(void) {
  return (ozni_limits){.steps = 1000000, .depth = 256, .nodes = 100000, .input_bytes = 1048576};
}

static bool any_error(const ozni_ctx *c) {
  for (ozni_index i = 0; i < ozni_table_count(c->diags); i++) {
    const ozni_diag *d = ozni_table_get(c->diags, i);
    if (d->severity == OZNI_SEV_ERROR) {
      return true;
    }
  }
  return false;
}

static void embedded_std(ozni_buf *b) {
  for (size_t k = 0; k < ozni_std_file_count; k++) {
    ozni_buf_puts(b, "// file: ");
    ozni_buf_puts(b, ozni_std_files[k].path);
    ozni_buf_puts(b, "\n");
    ozni_buf_put(b, ozni_std_files[k].text, ozni_std_files[k].len);
  }
}

char *ozni_run_text(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                    const ozni_source *srcs, size_t n, ozni_mode mode, const char *entry, int *has_errors) {
  *has_errors = 0;
  ozni_buf b = {.a = a};
  if (mode == OZNI_MODE_EMBEDDED_STD) {
    embedded_std(&b);
    return ozni_buf_finish(&b);
  }
  ozni_ctx c;
  if (!ozni_ctx_init(&c, a, lim, srcs, n)) {
    return nullptr;
  }
  if (set != nullptr) {
    c.set = *set;
  }
  ozni_project pr;
  bool loaded = ozni_project_load(&c, srcs, n, &pr);
  bool steps = mode == OZNI_MODE_STEPS || mode == OZNI_MODE_REQUIRES || mode == OZNI_MODE_IR ||
               mode == OZNI_MODE_SCHEDULE;
  if (loaded && !c.oom && !pr.syntax_errors && !c.limit_hit && (mode == OZNI_MODE_CHECK || steps)) {
    ozni_sema_out what = mode == OZNI_MODE_STEPS      ? OZNI_OUT_STEPS
                         : mode == OZNI_MODE_REQUIRES ? OZNI_OUT_REQUIRES
                         : mode == OZNI_MODE_IR       ? OZNI_OUT_IR
                         : mode == OZNI_MODE_SCHEDULE ? OZNI_OUT_SCHEDULE
                                                      : OZNI_OUT_NONE;
    loaded = ozni_sema_run(&pr, what, steps ? &b : nullptr, nullptr);
  }
  if (loaded && !c.oom && (mode == OZNI_MODE_CHECK || steps) && !ozni_apply_levels(&pr)) {
    c.oom = true;
  }
  if (!loaded || c.oom || ozni_arena_failures(c.arena) > 0) {
    c.oom = true;
    ozni_diag_add(&c, OZNI_D_MAC_outOfMemory, (ozni_span){0, 0, 0}, 0, nullptr, 0);
  }
  char *out = nullptr;
  bool errors = false;
  bool done = false;
  if (!c.oom && mode == OZNI_MODE_MODULES && !any_error(&c)) {
    done = ozni_render_modules(&pr, &b);
  } else if (!c.oom && mode == OZNI_MODE_DEFINITIONS && !any_error(&c)) {
    done = ozni_render_definitions(&pr, &b);
  } else if (!c.oom && mode == OZNI_MODE_ENTRY && !any_error(&c)) {
    bool ok = false;
    done = ozni_render_entry(&pr, entry, &b, &ok) && ok;
    if (!done) {
      ozni_buf_discard(&b);
    }
  }
  if (done) {
    errors = any_error(&c);
    out = ozni_buf_finish(&b);
  } else {
    if (!steps || c.oom) {
      ozni_buf_discard(&b);
    }
    if (ozni_render_check(&c, &b, &errors)) {
      out = ozni_buf_finish(&b);
    } else {
      ozni_buf_discard(&b);
    }
  }
  *has_errors = errors;
  ozni_ctx_free(&c);
  return out;
}

char *ozni_run_json(const ozni_allocator *a, const ozni_limits *lim, const ozni_settings *set,
                    const ozni_source *srcs, size_t n, unsigned emit, int *has_errors) {
  *has_errors = 0;
  ozni_ctx c;
  if (!ozni_ctx_init(&c, a, lim, srcs, n)) {
    return nullptr;
  }
  if (set != nullptr) {
    c.set = *set;
  }
  c.set.json = true;
  c.set.api = true;
  ozni_project pr;
  ozni_buf sched = {.a = a}, ir = {.a = a}, steps = {.a = a};
  c.ir_json = (emit & 1u) != 0 ? &ir : nullptr;
  c.steps_json = (emit & 4u) != 0 ? &steps : nullptr;
  bool loaded = ozni_project_load(&c, srcs, n, &pr);
  if (loaded && !c.oom && !pr.syntax_errors && !c.limit_hit) {
    loaded = ozni_sema_run(&pr, (emit & 6u) != 0 ? OZNI_OUT_SCHEDULE : OZNI_OUT_NONE, &sched, nullptr);
  }
  if (loaded && !c.oom && !ozni_apply_levels(&pr)) {
    c.oom = true;
  }
  if (!loaded || c.oom || ozni_arena_failures(c.arena) > 0) {
    c.oom = true;
    ozni_diag_add(&c, OZNI_D_MAC_outOfMemory, (ozni_span){0, 0, 0}, 0, nullptr, 0);
  }
  ozni_buf b = {.a = a};
  bool errors = false;
  char *out = nullptr;
  if (ozni_render_result_head(&c, &b, &errors)) {
    const ozni_buf *parts[3] = {&ir, &sched, &steps};
    static const char *const keys[3] = {",\"ir\":[", ",\"schedules\":[", ",\"steps\":["};
    static const unsigned bits[3] = {1u, 2u, 4u};
    for (int k = 0; k < 3; k++) {
      if ((emit & bits[k]) != 0) {
        ozni_buf_puts(&b, keys[k]);
        bool ok = !c.oom && !parts[k]->failed && parts[k]->p != nullptr;
        ozni_buf_put(&b, ok ? parts[k]->p : "", ok ? parts[k]->len : 0);
        ozni_buf_puts(&b, "]");
      }
    }
    if (loaded && !c.oom && (emit & 56u) != 0 && !ozni_render_public(&pr, &b, emit)) {
      b.failed = true;
    }
    ozni_buf_puts(&b, "}\n");
    out = ozni_buf_finish(&b);
  } else {
    ozni_buf_discard(&b);
  }
  ozni_buf_discard(&sched);
  ozni_buf_discard(&ir);
  ozni_buf_discard(&steps);
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
  size_t bad = 0;
  bool admitted = src->len <= lim->input_bytes && ozni_check_utf8(src->text, src->len, &bad);
  if (src->len > lim->input_bytes) {
    ozni_diag_add(&c, OZNI_D_SYN_inputTooLarge, (ozni_span){0, 0, 0}, 0, nullptr, 0);
  } else if (!admitted) {
    ozni_diag_add(&c, OZNI_D_SYN_invalidUtf8, (ozni_span){0, (uint32_t)bad, (uint32_t)bad + 1}, 0, nullptr, 0);
  }
  bool ok = admitted && ozni_parse(&c, 0, &ast);
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
  *has_errors = any_error(&c);
  ozni_ctx_free(&c);
  return out;
}

void ozni_free_text(const ozni_allocator *a, char *text) {
  if (text != nullptr) {
    a->release(a->ctx, text, strlen(text) + 1);
  }
}
