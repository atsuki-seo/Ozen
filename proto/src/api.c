/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "core.h"

ozp_limits ozp_default_limits(void) {
  return (ozp_limits){.steps = 1000000, .depth = 256, .nodes = 100000, .input_bytes = 1048576};
}

bool ozpi_put_escaped(ozpi_ctx *c, ozpi_buf *b, ozpi_str s) {
  size_t start = 0;
  for (size_t i = 0; i < s.len; i++) {
    const char *rep = s.p[i] == '"' ? "\\\"" : s.p[i] == '\\' ? "\\\\" : s.p[i] == '\n' ? "\\n" : nullptr;
    if (rep != nullptr) {
      if (!ozpi_buf_put(c, b, s.p + start, i - start) || !ozpi_buf_puts(c, b, rep)) {
        return false;
      }
      start = i + 1;
    }
  }
  return ozpi_buf_put(c, b, s.p + start, s.len - start);
}

static bool put_pos(ozpi_ctx *c, ozpi_buf *b, ozpi_span sp) {
  return ozpi_buf_putu(c, b, sp.start.line) && ozpi_buf_puts(c, b, ":") && ozpi_buf_putu(c, b, sp.start.col) &&
         ozpi_buf_puts(c, b, "-") && ozpi_buf_putu(c, b, sp.end.line) && ozpi_buf_puts(c, b, ":") &&
         ozpi_buf_putu(c, b, sp.end.col);
}

static bool render_diag(ozpi_ctx *c, ozpi_buf *b, const ozpi_diag *d) {
  bool ok = ozpi_buf_puts(c, b, "error ") && ozpi_buf_puts(c, b, ozpi_diag_code(d->id)) && ozpi_buf_puts(c, b, " ") &&
            ozpi_buf_puts(c, b, ozpi_diag_name(d->id)) && ozpi_buf_puts(c, b, " ") && put_pos(c, b, d->span);
  if (ok && d->nargs > 0) {
    ok = ozpi_buf_puts(c, b, " args=[");
    for (uint32_t i = 0; ok && i < d->nargs; i++) {
      ok = (i == 0 || ozpi_buf_puts(c, b, ", ")) && ozpi_buf_puts(c, b, "\"") && ozpi_put_escaped(c, b, d->args[i]) &&
           ozpi_buf_puts(c, b, "\"");
    }
    ok = ok && ozpi_buf_puts(c, b, "]");
  }
  bool first = true;
  for (uint32_t e = d->exp; ok && e != 0 && e < c->nexps; e = c->exps[e].parent) {
    ok = ozpi_buf_puts(c, b, first ? " via " : ", ") && put_pos(c, b, c->exps[e].call);
    first = false;
  }
  return ok && ozpi_buf_puts(c, b, "\n");
}

static bool render(ozpi_ctx *c, ozpi_buf *b, bool stats) {
  bool ok = true;
  for (uint32_t i = 0; ok && i < c->nsteps; i++) {
    ok = ozpi_buf_puts(c, b, "%") && ozpi_buf_putu(c, b, i + 1) && ozpi_buf_puts(c, b, " = ") &&
         ozpi_buf_put(c, b, c->steps[i].call.p, c->steps[i].call.len) &&
         ozpi_buf_put(c, b, c->steps[i].annots.p, c->steps[i].annots.len) && ozpi_buf_puts(c, b, "\n");
  }
  for (uint32_t i = 0; ok && i < c->ndiags; i++) {
    ok = render_diag(c, b, &c->diags[i]);
  }
  ok = ok && ozpi_buf_puts(c, b, c->ndiags == 0 ? "result ok steps=" : "result error steps=") &&
       ozpi_buf_putu(c, b, c->nsteps) && ozpi_buf_puts(c, b, " diagnostics=") && ozpi_buf_putu(c, b, c->ndiags) &&
       ozpi_buf_puts(c, b, "\n");
  if (ok && stats) {
    ok = ozpi_buf_puts(c, b, "stats steps=") && ozpi_buf_putu(c, b, c->fuel) && ozpi_buf_puts(c, b, "/") &&
         ozpi_buf_putu(c, b, c->lim.steps) && ozpi_buf_puts(c, b, " depth=") && ozpi_buf_putu(c, b, c->maxdepth) &&
         ozpi_buf_puts(c, b, "/") && ozpi_buf_putu(c, b, c->lim.depth) && ozpi_buf_puts(c, b, " nodes=") &&
         ozpi_buf_putu(c, b, c->nodes) && ozpi_buf_puts(c, b, "/") && ozpi_buf_putu(c, b, c->lim.nodes) &&
         ozpi_buf_puts(c, b, "\n");
  }
  return ok;
}

static bool has_syntax_error(const ozpi_ctx *c) {
  for (uint32_t i = 0; i < c->ndiags; i++) {
    if (c->diags[i].id <= OZPI_D_SYN_macroBody) {
      return true;
    }
  }
  return false;
}

char *ozp_expand(const char *src, size_t src_len, const ozp_options *opt) {
  ozpi_ctx c = {0};
  ozpi_arena_init(&c.ar, opt->allocator);
  c.lim = opt->limits;
  const ozpi_span none = {{1, 1}, {1, 1}};
  if (!ozpi_grow(&c, (void **)&c.diags, &c.capdiags, sizeof(ozpi_diag), 8)) {
    ozpi_arena_free_all(&c.ar);
    return nullptr;
  }
  ozpi_pos bad;
  if (src_len > c.lim.input_bytes) {
    ozpi_diag2(&c, OZPI_D_SYN_inputTooLarge, none, 0, ozpi_fmt_u(&c, src_len), ozpi_fmt_u(&c, c.lim.input_bytes));
  } else if (!ozpi_check_utf8(src, src_len, &bad)) {
    ozpi_diag0(&c, OZPI_D_SYN_invalidUtf8, (ozpi_span){bad, (ozpi_pos){bad.line, bad.col + 1}}, 0);
  } else {
    ozpi_parse_file(&c, src, src_len);
    if (!c.oom) {
      ozpi_check_macros(&c);
    }
    if (!c.oom && !has_syntax_error(&c)) {
      ozpi_evaluate(&c, opt->entry, opt->params);
    }
  }
  ozpi_buf out = {0};
  char *result = nullptr;
  if (render(&c, &out, opt->stats != 0)) {
    result = opt->allocator->alloc(opt->allocator->ctx, out.len + 1);
    if (result != nullptr) {
      memcpy(result, out.p, out.len + 1);
    }
  }
  ozpi_arena_free_all(&c.ar);
  return result;
}

void ozp_free_string(const ozp_options *opt, char *s) {
  if (s != nullptr) {
    opt->allocator->release(opt->allocator->ctx, s, strlen(s) + 1);
  }
}
