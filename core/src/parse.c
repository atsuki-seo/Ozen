/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "ast.h"
#include "lex.h"

typedef enum body_kind : uint8_t { BODY_PLAIN, BODY_ACTION, BODY_VARIANT } body_kind;
typedef enum block_kind : uint8_t { BLOCK_PLAIN, BLOCK_RECIPE, BLOCK_MENU, BLOCK_MACRO } block_kind;

typedef struct parser {
  ozni_ctx *c;
  ozni_index file;
  ozni_ast *ast;
  ozni_table *toks;
  ozni_index pos;
  uint32_t prev_end;
  uint32_t depth;
  bool depth_reported;
  bool failed;
  bool oom;
  bool no_construct;
  int quote_depth;
} parser;

static const ozni_token *tok_at(const parser *p, ozni_index k) {
  ozni_index n = ozni_table_count(p->toks);
  ozni_index i = p->pos + k < n ? p->pos + k : n - 1;
  return ozni_table_get(p->toks, i);
}

static const ozni_token *cur(const parser *p) { return tok_at(p, 0); }
static ozni_tok_kind kind_at(const parser *p, ozni_index k) { return tok_at(p, k)->kind; }
static bool at(const parser *p, ozni_tok_kind k) { return cur(p)->kind == k; }

static bool text_is(const parser *p, const ozni_token *t, const char *s) {
  ozni_str w = ozni_intern_get(p->c->names, t->text);
  size_t n = strlen(s);
  return t->text != 0 && w.len == n && memcmp(w.p, s, n) == 0;
}

static bool at_kw(const parser *p, const char *s) { return at(p, OZNI_T_KEYWORD) && text_is(p, cur(p), s); }
static bool at_word(const parser *p, const char *s) { return at(p, OZNI_T_IDENT) && text_is(p, cur(p), s); }

static void advance(parser *p) {
  const ozni_token *t = cur(p);
  if (t->kind != OZNI_T_EOF) {
    p->prev_end = t->end;
    p->pos++;
  }
}

static void mark(parser *p, ozni_rule r) { p->ast->rules[r / 8] |= (uint8_t)(1u << (r % 8)); }

static ozni_node *N(parser *p, ozni_index i) { return ozni_table_at(p->ast->nodes, i); }

static ozni_index node(parser *p, ozni_node_kind kind, uint32_t start) {
  ozni_node n = {.kind = kind, .span = {p->file, start, start}};
  ozni_index i = 0;
  if (!ozni_table_push(p->ast->nodes, &n, &i)) {
    p->oom = true;
    p->failed = true;
    return 0;
  }
  return i;
}

static ozni_index node_here(parser *p, ozni_node_kind kind) { return node(p, kind, cur(p)->start); }

static ozni_index finish(parser *p, ozni_index n) {
  if (n != 0) {
    N(p, n)->span.end = p->prev_end > N(p, n)->span.start ? p->prev_end : N(p, n)->span.start;
  }
  return n;
}

static void add(parser *p, ozni_index parent, ozni_index child) {
  if (parent == 0 || child == 0) {
    return;
  }
  ozni_node *pn = N(p, parent);
  if (pn->last == 0) {
    pn->first = child;
  } else {
    N(p, pn->last)->next = child;
  }
  pn->last = child;
}

static void set_text(parser *p, ozni_index n, ozni_index text) {
  if (n != 0) {
    N(p, n)->text = text;
  }
}

static bool put_text(parser *p, const char *s, size_t len, ozni_index *out) {
  if (!ozni_intern_put(p->c->names, s, len, out)) {
    p->oom = true;
    p->failed = true;
    return false;
  }
  return true;
}

static void diag_at(parser *p, ozni_diag_id id, uint32_t start, uint32_t end, const ozni_str *args, uint16_t nargs) {
  if (!ozni_diag_add(p->c, id, (ozni_span){p->file, start, end}, 0, args, nargs)) {
    p->oom = true;
  }
}

static void unexpected(parser *p) {
  if (p->failed) {
    return;
  }
  const ozni_token *t = cur(p);
  ozni_str arg;
  if (t->kind == OZNI_T_EOF) {
    arg = (ozni_str){"end of input", 12};
  } else if (t->kind == OZNI_T_SEP && t->text == 0) {
    arg = (ozni_str){"newline", 7};
  } else {
    const char *s = p->c->srcs[p->file].text;
    arg = (ozni_str){s + t->start, t->end - t->start};
  }
  diag_at(p, t->kind == OZNI_T_KEYWORD ? OZNI_D_SYN_reservedWord : OZNI_D_SYN_unexpectedToken, t->start, t->end, &arg,
          1);
  p->failed = true;
}

static bool expect(parser *p, ozni_tok_kind k) {
  if (p->failed) {
    return false;
  }
  if (!at(p, k)) {
    unexpected(p);
    return false;
  }
  advance(p);
  return true;
}

static ozni_index expect_ident(parser *p) {
  if (p->failed) {
    return 0;
  }
  if (!at(p, OZNI_T_IDENT)) {
    unexpected(p);
    return 0;
  }
  ozni_index text = cur(p)->text;
  advance(p);
  return text;
}

static ozni_index name_of(parser *p, ozni_index n) {
  if (n != 0 && !p->failed) {
    N(p, n)->name_start = cur(p)->start;
    N(p, n)->name_end = cur(p)->end;
  }
  return expect_ident(p);
}

static bool enter(parser *p) {
  if (p->failed) {
    return false;
  }
  if (++p->depth > p->c->lim.depth) {
    if (!p->depth_reported) {
      char num[24];
      size_t n = 0;
      char rev[24];
      uint32_t v = p->c->lim.depth;
      do {
        rev[n++] = (char)('0' + v % 10);
        v /= 10;
      } while (v != 0);
      for (size_t k = 0; k < n; k++) {
        num[k] = rev[n - 1 - k];
      }
      ozni_str arg = {num, (uint32_t)n};
      diag_at(p, OZNI_D_SYN_nestingTooDeep, cur(p)->start, cur(p)->end, &arg, 1);
      p->depth_reported = true;
    }
    p->failed = true;
    p->depth--;
    return false;
  }
  return true;
}

static void leave(parser *p) { p->depth--; }

static void sync(parser *p) {
  int depth = 0;
  while (!at(p, OZNI_T_EOF)) {
    ozni_tok_kind k = cur(p)->kind;
    if (depth == 0 && (k == OZNI_T_SEP || k == OZNI_T_RBRACE)) {
      return;
    }
    if (k == OZNI_T_LBRACE || k == OZNI_T_LPAREN || k == OZNI_T_LBRACKET) {
      depth++;
    } else if ((k == OZNI_T_RBRACE || k == OZNI_T_RPAREN || k == OZNI_T_RBRACKET) && depth > 0) {
      depth--;
    }
    advance(p);
  }
}

static void skip_seps(parser *p) {
  while (at(p, OZNI_T_SEP)) {
    advance(p);
  }
}

static ozni_index expr(parser *p);
static ozni_index sum(parser *p);
static ozni_index postfix(parser *p);
static ozni_index type(parser *p);
static ozni_index type_ref(parser *p);
static ozni_index block(parser *p, block_kind kind);
static ozni_index decl_body(parser *p, body_kind kind);
static bool item(parser *p, ozni_index parent);

static void annotations(parser *p, ozni_index target);

static bool list_sep(parser *p) {
  if (!at(p, OZNI_T_COMMA) && !at(p, OZNI_T_SEP)) {
    return false;
  }
  mark(p, OZNI_R_list_sep);
  advance(p);
  skip_seps(p);
  return true;
}

static ozni_index qual_name(parser *p) {
  uint32_t start = cur(p)->start;
  if (!at(p, OZNI_T_IDENT)) {
    unexpected(p);
    return 0;
  }
  mark(p, OZNI_R_qual_name);
  const char *s = p->c->srcs[p->file].text;
  advance(p);
  while (at(p, OZNI_T_DOT) && kind_at(p, 1) == OZNI_T_IDENT) {
    advance(p);
    advance(p);
  }
  ozni_index text = 0;
  put_text(p, s + start, p->prev_end - start, &text);
  return text;
}

static ozni_index named_or_positional(parser *p, bool *named) {
  *named = false;
  if ((at(p, OZNI_T_IDENT) || at(p, OZNI_T_KEYWORD)) && kind_at(p, 1) == OZNI_T_COLON) {
    mark(p, OZNI_R_named_arg);
    mark(p, OZNI_R_word);
    ozni_index n = node_here(p, OZNI_N_NamedArg);
    set_text(p, n, cur(p)->text);
    advance(p);
    advance(p);
    add(p, n, expr(p));
    *named = true;
    return finish(p, n);
  }
  return expr(p);
}

static void args(parser *p, ozni_index parent) {
  mark(p, OZNI_R_args);
  bool seen_named = false;
  if (at(p, OZNI_T_RPAREN)) {
    return;
  }
  for (;;) {
    bool named = false;
    uint32_t start = cur(p)->start;
    ozni_index a = named_or_positional(p, &named);
    if (a == 0) {
      return;
    }
    if (!named && seen_named) {
      diag_at(p, OZNI_D_SYN_argumentOrder, start, p->prev_end, nullptr, 0);
    }
    seen_named = seen_named || named;
    add(p, parent, a);
    if (!at(p, OZNI_T_COMMA)) {
      return;
    }
    advance(p);
  }
}

static void call_args(parser *p, ozni_index parent) {
  if (!enter(p)) {
    return;
  }
  advance(p);
  args(p, parent);
  expect(p, OZNI_T_RPAREN);
  leave(p);
}

static void annotations(parser *p, ozni_index target) {
  mark(p, OZNI_R_annotations);
  while (at(p, OZNI_T_AT) && !p->failed) {
    mark(p, OZNI_R_annotation);
    ozni_index n = node_here(p, OZNI_N_Annotation);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (at(p, OZNI_T_LPAREN)) {
      call_args(p, n);
    }
    add(p, target, finish(p, n));
    finish(p, target);
  }
}

static bool is_amount_start(const parser *p, ozni_index k) {
  ozni_tok_kind t = kind_at(p, k);
  return t == OZNI_T_NUMBER || t == OZNI_T_FRACTION;
}

static bool is_unit_start(const parser *p, ozni_index k) {
  return kind_at(p, k) == OZNI_T_IDENT || kind_at(p, k) == OZNI_T_PERCENT;
}

static bool amount(parser *p, uint32_t *start, uint32_t *end) {
  mark(p, OZNI_R_amount);
  mark(p, OZNI_R_number);
  *start = cur(p)->start;
  advance(p);
  if (at(p, OZNI_T_DOTDOT) && is_amount_start(p, 1)) {
    advance(p);
    mark(p, OZNI_R_number);
    advance(p);
  }
  *end = p->prev_end;
  return true;
}

static bool unit_expr(parser *p, uint32_t *end) {
  mark(p, OZNI_R_unit_expr);
  if (at(p, OZNI_T_PERCENT)) {
    advance(p);
    *end = p->prev_end;
    return true;
  }
  advance(p);
  while (at(p, OZNI_T_SLASH) && cur(p)->start == p->prev_end && kind_at(p, 1) == OZNI_T_IDENT &&
         tok_at(p, 1)->start == cur(p)->end) {
    advance(p);
    advance(p);
  }
  *end = p->prev_end;
  return true;
}

static ozni_index quantity_or_number(parser *p) {
  const char *s = p->c->srcs[p->file].text;
  uint32_t start = cur(p)->start;
  uint32_t as, ae;
  bool range = at(p, OZNI_T_NUMBER) && kind_at(p, 1) == OZNI_T_DOTDOT && is_amount_start(p, 2);
  amount(p, &as, &ae);
  if (!is_unit_start(p, 0)) {
    mark(p, OZNI_R_literal);
    ozni_index n = node(p, range ? OZNI_N_NumberRange : OZNI_N_Number, start);
    ozni_index text = 0;
    put_text(p, s + as, ae - as, &text);
    set_text(p, n, text);
    return finish(p, n);
  }
  mark(p, OZNI_R_quantity);
  ozni_index q = node(p, OZNI_N_Quantity, start);
  for (;;) {
    uint32_t ue;
    uint32_t us = cur(p)->start;
    unit_expr(p, &ue);
    ozni_index part = node(p, OZNI_N_QtyPart, as);
    ozni_index at_text = 0, unit_text = 0;
    put_text(p, s + as, ae - as, &at_text);
    put_text(p, s + us, ue - us, &unit_text);
    if (part != 0) {
      N(p, part)->text = at_text;
      N(p, part)->text2 = unit_text;
    }
    add(p, q, finish(p, part));
    if (!(is_amount_start(p, 0) && (is_unit_start(p, 1) || (kind_at(p, 1) == OZNI_T_DOTDOT && is_amount_start(p, 2) &&
                                                              is_unit_start(p, 3))))) {
      break;
    }
    amount(p, &as, &ae);
  }
  return finish(p, q);
}

static ozni_index string_node(parser *p) {
  mark(p, OZNI_R_literal);
  ozni_index n = node_here(p, at(p, OZNI_T_MLSTRING) ? OZNI_N_MlString : OZNI_N_String);
  set_text(p, n, cur(p)->text);
  advance(p);
  return finish(p, n);
}

static ozni_index items_into(parser *p, ozni_index parent, ozni_tok_kind close);

static ozni_index record_into(parser *p, ozni_index n) {
  mark(p, OZNI_R_record);
  if (!enter(p)) {
    return 0;
  }
  advance(p);
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !p->failed) {
    if (!at(p, OZNI_T_IDENT) && !at(p, OZNI_T_KEYWORD)) {
      unexpected(p);
      break;
    }
    mark(p, OZNI_R_record_entry);
    mark(p, OZNI_R_word);
    ozni_index e = node_here(p, OZNI_N_Entry);
    set_text(p, e, cur(p)->text);
    advance(p);
    if (!expect(p, OZNI_T_COLON)) {
      break;
    }
    add(p, e, expr(p));
    add(p, n, finish(p, e));
    if (!list_sep(p)) {
      break;
    }
  }
  skip_seps(p);
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index primary(parser *p) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_primary);
  const ozni_token *t = cur(p);
  switch (t->kind) {
  case OZNI_T_NUMBER:
  case OZNI_T_FRACTION:
    return quantity_or_number(p);
  case OZNI_T_STRING:
  case OZNI_T_MLSTRING:
    return string_node(p);
  case OZNI_T_DOLLAR: {
    mark(p, OZNI_R_splice);
    ozni_index n = node_here(p, OZNI_N_Splice);
    advance(p);
    set_text(p, n, name_of(p, n));
    finish(p, n);
    if (p->quote_depth == 0 && !p->failed) {
      diag_at(p, OZNI_D_SYN_spliceOutsideQuote, N(p, n)->span.start, N(p, n)->span.end, nullptr, 0);
    }
    return p->failed ? 0 : n;
  }
  case OZNI_T_LBRACKET: {
    if (kind_at(p, 1) == OZNI_T_IDENT && text_is(p, tok_at(p, 1), "any") && kind_at(p, 2) == OZNI_T_IDENT &&
        text_is(p, tok_at(p, 2), "amount") && kind_at(p, 3) == OZNI_T_RBRACKET) {
      mark(p, OZNI_R_vague_amount);
      ozni_index n = node_here(p, OZNI_N_Vague);
      advance(p);
      advance(p);
      advance(p);
      advance(p);
      return finish(p, n);
    }
    mark(p, OZNI_R_list);
    ozni_index n = node_here(p, OZNI_N_List);
    if (!enter(p)) {
      return 0;
    }
    advance(p);
    if (!at(p, OZNI_T_RBRACKET)) {
      for (;;) {
        mark(p, OZNI_R_item_expr);
        ozni_index e = expr(p);
        annotations(p, e);
        add(p, n, e);
        if (p->failed || !at(p, OZNI_T_COMMA)) {
          break;
        }
        advance(p);
      }
    }
    expect(p, OZNI_T_RBRACKET);
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  case OZNI_T_LBRACE:
    return record_into(p, node_here(p, OZNI_N_Record));
  case OZNI_T_LPAREN: {
    mark(p, OZNI_R_paren);
    if (!enter(p)) {
      return 0;
    }
    advance(p);
    bool saved = p->no_construct;
    p->no_construct = false;
    ozni_index e = expr(p);
    p->no_construct = saved;
    expect(p, OZNI_T_RPAREN);
    leave(p);
    return p->failed ? 0 : e;
  }
  case OZNI_T_IDENT: {
    if (kind_at(p, 1) == OZNI_T_LBRACE && !p->no_construct) {
      mark(p, OZNI_R_construct);
      ozni_index n = node_here(p, OZNI_N_Construct);
      set_text(p, n, t->text);
      advance(p);
      return record_into(p, n);
    }
    mark(p, OZNI_R_name);
    ozni_index n = node_here(p, OZNI_N_Name);
    set_text(p, n, t->text);
    advance(p);
    return finish(p, n);
  }
  case OZNI_T_KEYWORD:
    if (at_kw(p, "true") || at_kw(p, "false")) {
      mark(p, OZNI_R_literal);
      ozni_index n = node_here(p, OZNI_N_Bool);
      set_text(p, n, t->text);
      advance(p);
      return finish(p, n);
    }
    if (at_kw(p, "quote")) {
      mark(p, OZNI_R_quote);
      ozni_index n = node_here(p, OZNI_N_Quote);
      if (p->quote_depth != -1) {
        diag_at(p, OZNI_D_SYN_quotePlacement, t->start, t->end, nullptr, 0);
      }
      int saved = p->quote_depth;
      p->quote_depth = saved < 0 ? 1 : saved + 1;
      advance(p);
      add(p, n, block(p, BLOCK_PLAIN));
      p->quote_depth = saved;
      return p->failed ? 0 : finish(p, n);
    }
    if (at_kw(p, "make")) {
      mark(p, OZNI_R_make);
      ozni_index n = node_here(p, OZNI_N_Make);
      advance(p);
      add(p, n, postfix(p));
      return p->failed ? 0 : finish(p, n);
    }
    if (at_kw(p, "buy")) {
      mark(p, OZNI_R_buy);
      ozni_index n = node_here(p, OZNI_N_Buy);
      advance(p);
      add(p, n, type_ref(p));
      if (is_amount_start(p, 0) && !p->failed) {
        add(p, n, quantity_or_number(p));
      }
      return p->failed ? 0 : finish(p, n);
    }
    if (at_kw(p, "choose") || at_kw(p, "assemble")) {
      bool choose = at_kw(p, "choose");
      mark(p, choose ? OZNI_R_choose : OZNI_R_assemble);
      ozni_index n = node_here(p, choose ? OZNI_N_Choose : OZNI_N_Assemble);
      advance(p);
      if (choose) {
        set_text(p, n, name_of(p, n));
        if (!p->failed && !at_word(p, "from")) {
          unexpected(p);
        }
        advance(p);
      }
      items_into(p, n, OZNI_T_RBRACE);
      return p->failed ? 0 : finish(p, n);
    }
    unexpected(p);
    return 0;
  case OZNI_T_EOF:
  case OZNI_T_SEP:
  case OZNI_T_RPAREN:
  case OZNI_T_RBRACE:
  case OZNI_T_RBRACKET:
  case OZNI_T_LT:
  case OZNI_T_GT:
  case OZNI_T_LE:
  case OZNI_T_GE:
  case OZNI_T_EQEQ:
  case OZNI_T_NE:
  case OZNI_T_EQ:
  case OZNI_T_COLON:
  case OZNI_T_COMMA:
  case OZNI_T_DOT:
  case OZNI_T_DOTDOT:
  case OZNI_T_PLUS:
  case OZNI_T_MINUS:
  case OZNI_T_STAR:
  case OZNI_T_SLASH:
  case OZNI_T_QUESTION:
  case OZNI_T_PIPE:
  case OZNI_T_AT:
  case OZNI_T_PERCENT:
  case OZNI_T_ARROW:
  case OZNI_T_LINK:
    break;
  }
  unexpected(p);
  return 0;
}

static ozni_index postfix(parser *p) {
  mark(p, OZNI_R_postfix);
  uint32_t start = cur(p)->start;
  ozni_index e = primary(p);
  while (e != 0 && !p->failed) {
    if (at(p, OZNI_T_LPAREN)) {
      mark(p, OZNI_R_call);
      ozni_index n = node(p, OZNI_N_Call, start);
      add(p, n, e);
      call_args(p, n);
      e = finish(p, n);
    } else if (at(p, OZNI_T_DOT) && kind_at(p, 1) == OZNI_T_IDENT) {
      mark(p, OZNI_R_member_access);
      ozni_index n = node(p, OZNI_N_Member, start);
      advance(p);
      set_text(p, n, cur(p)->text);
      advance(p);
      add(p, n, e);
      e = finish(p, n);
    } else {
      break;
    }
  }
  return p->failed ? 0 : e;
}

static ozni_index unary(parser *p) {
  mark(p, OZNI_R_unary);
  if (at(p, OZNI_T_MINUS)) {
    if (!enter(p)) {
      return 0;
    }
    ozni_index n = node_here(p, OZNI_N_Unary);
    ozni_index op = 0;
    put_text(p, "-", 1, &op);
    set_text(p, n, op);
    advance(p);
    add(p, n, unary(p));
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  return postfix(p);
}

static ozni_index binary(parser *p, uint32_t start, const char *op, ozni_index lhs, ozni_index rhs) {
  ozni_index n = node(p, OZNI_N_Binary, start);
  ozni_index text = 0;
  put_text(p, op, strlen(op), &text);
  set_text(p, n, text);
  add(p, n, lhs);
  add(p, n, rhs);
  return finish(p, n);
}

static ozni_index product(parser *p) {
  mark(p, OZNI_R_product);
  uint32_t start = cur(p)->start;
  ozni_index e = unary(p);
  while (e != 0 && (at(p, OZNI_T_STAR) || at(p, OZNI_T_SLASH))) {
    const char *op = at(p, OZNI_T_STAR) ? "*" : "/";
    advance(p);
    ozni_index r = unary(p);
    if (r == 0) {
      return 0;
    }
    e = binary(p, start, op, e, r);
  }
  return p->failed ? 0 : e;
}

static ozni_index sum(parser *p) {
  mark(p, OZNI_R_sum);
  uint32_t start = cur(p)->start;
  ozni_index e = product(p);
  while (e != 0 && (at(p, OZNI_T_PLUS) || at(p, OZNI_T_MINUS))) {
    const char *op = at(p, OZNI_T_PLUS) ? "+" : "-";
    advance(p);
    ozni_index r = product(p);
    if (r == 0) {
      return 0;
    }
    e = binary(p, start, op, e, r);
  }
  return p->failed ? 0 : e;
}

static bool sum_start(const parser *p) {
  switch (cur(p)->kind) {
  case OZNI_T_NUMBER:
  case OZNI_T_FRACTION:
  case OZNI_T_STRING:
  case OZNI_T_MLSTRING:
  case OZNI_T_IDENT:
  case OZNI_T_DOLLAR:
  case OZNI_T_LPAREN:
  case OZNI_T_LBRACKET:
  case OZNI_T_MINUS:
    return true;
  case OZNI_T_KEYWORD:
    return at_kw(p, "true") || at_kw(p, "false") || at_kw(p, "make") || at_kw(p, "buy") || at_kw(p, "choose") ||
           at_kw(p, "assemble") || at_kw(p, "quote");
  case OZNI_T_EOF:
  case OZNI_T_SEP:
  case OZNI_T_RPAREN:
  case OZNI_T_LBRACE:
  case OZNI_T_RBRACE:
  case OZNI_T_RBRACKET:
  case OZNI_T_LT:
  case OZNI_T_GT:
  case OZNI_T_LE:
  case OZNI_T_GE:
  case OZNI_T_EQEQ:
  case OZNI_T_NE:
  case OZNI_T_EQ:
  case OZNI_T_COLON:
  case OZNI_T_COMMA:
  case OZNI_T_DOT:
  case OZNI_T_DOTDOT:
  case OZNI_T_PLUS:
  case OZNI_T_STAR:
  case OZNI_T_SLASH:
  case OZNI_T_QUESTION:
  case OZNI_T_PIPE:
  case OZNI_T_AT:
  case OZNI_T_PERCENT:
  case OZNI_T_ARROW:
  case OZNI_T_LINK:
    return false;
  }
  return false;
}

static ozni_index range(parser *p) {
  mark(p, OZNI_R_range);
  uint32_t start = cur(p)->start;
  if (at(p, OZNI_T_DOTDOT)) {
    ozni_index n = node_here(p, OZNI_N_RangeTo);
    advance(p);
    add(p, n, sum(p));
    return p->failed ? 0 : finish(p, n);
  }
  ozni_index lo = sum(p);
  if (lo == 0 || !at(p, OZNI_T_DOTDOT)) {
    return lo;
  }
  advance(p);
  if (!sum_start(p)) {
    ozni_index n = node(p, OZNI_N_RangeFrom, start);
    add(p, n, lo);
    return finish(p, n);
  }
  ozni_index n = node(p, OZNI_N_Range, start);
  add(p, n, lo);
  add(p, n, sum(p));
  return p->failed ? 0 : finish(p, n);
}

static ozni_index compare(parser *p) {
  mark(p, OZNI_R_compare);
  uint32_t start = cur(p)->start;
  ozni_index e = range(p);
  static const struct {
    ozni_tok_kind k;
    const char *op;
  } ops[] = {{OZNI_T_EQEQ, "=="}, {OZNI_T_NE, "!="}, {OZNI_T_LT, "<"},
             {OZNI_T_LE, "<="},   {OZNI_T_GT, ">"},  {OZNI_T_GE, ">="}};
  for (size_t i = 0; e != 0 && i < sizeof ops / sizeof ops[0]; i++) {
    if (at(p, ops[i].k)) {
      advance(p);
      ozni_index r = range(p);
      return r == 0 ? 0 : binary(p, start, ops[i].op, e, r);
    }
  }
  return e;
}

static ozni_index not_expr(parser *p) {
  mark(p, OZNI_R_not_expr);
  if (at_kw(p, "not")) {
    if (!enter(p)) {
      return 0;
    }
    ozni_index n = node_here(p, OZNI_N_Not);
    advance(p);
    add(p, n, not_expr(p));
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  return compare(p);
}

static ozni_index logic(parser *p, const char *word, ozni_rule rule, ozni_index (*next)(parser *)) {
  mark(p, rule);
  uint32_t start = cur(p)->start;
  ozni_index e = next(p);
  while (e != 0 && at_kw(p, word)) {
    advance(p);
    ozni_index r = next(p);
    if (r == 0) {
      return 0;
    }
    e = binary(p, start, word, e, r);
  }
  return p->failed ? 0 : e;
}

static ozni_index and_expr(parser *p) { return logic(p, "and", OZNI_R_and_expr, not_expr); }
static ozni_index or_expr(parser *p) { return logic(p, "or", OZNI_R_or_expr, and_expr); }

static ozni_index expr(parser *p) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_expr);
  mark(p, OZNI_R_link);
  uint32_t start = cur(p)->start;
  ozni_index e = or_expr(p);
  while (e != 0 && at(p, OZNI_T_LINK)) {
    advance(p);
    ozni_index r = or_expr(p);
    if (r == 0) {
      return 0;
    }
    e = binary(p, start, "<->", e, r);
  }
  return p->failed ? 0 : e;
}

static ozni_index expr_no_construct(parser *p) {
  bool saved = p->no_construct;
  p->no_construct = true;
  ozni_index e = expr(p);
  p->no_construct = saved;
  return e;
}

static ozni_index items_into(parser *p, ozni_index parent, ozni_tok_kind close) {
  (void)close;
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  mark(p, OZNI_R_items);
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !p->failed) {
    mark(p, OZNI_R_item_expr);
    ozni_index e = expr(p);
    annotations(p, e);
    add(p, parent, e);
    if (!list_sep(p)) {
      break;
    }
  }
  skip_seps(p);
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : parent;
}

/* Types */

static ozni_index type_args(parser *p, ozni_index parent) {
  mark(p, OZNI_R_type_args);
  if (!enter(p)) {
    return 0;
  }
  ozni_index ta = node_here(p, OZNI_N_TypeArgs);
  advance(p);
  for (;;) {
    mark(p, OZNI_R_type_alt);
    ozni_index alt = node_here(p, OZNI_N_Alt);
    for (;;) {
      mark(p, OZNI_R_type_arg);
      if (!at(p, OZNI_T_IDENT)) {
        unexpected(p);
        break;
      }
      ozni_index name = cur(p)->text;
      uint32_t start = cur(p)->start;
      const char *op = nullptr;
      if (kind_at(p, 1) == OZNI_T_COLON) {
        op = ":";
      } else if (kind_at(p, 1) == OZNI_T_GE) {
        op = ">=";
      } else if (kind_at(p, 1) == OZNI_T_LE) {
        op = "<=";
      } else if (kind_at(p, 1) == OZNI_T_IDENT && text_is(p, tok_at(p, 1), "has")) {
        op = "has";
      }
      advance(p);
      ozni_index a;
      if (op != nullptr) {
        mark(p, OZNI_R_state_arg);
        a = node(p, OZNI_N_StateArg, start);
        set_text(p, a, name);
        ozni_index op_text = 0;
        put_text(p, op, strlen(op), &op_text);
        if (a != 0) {
          N(p, a)->text2 = op_text;
        }
        advance(p);
        add(p, a, sum(p));
      } else {
        mark(p, OZNI_R_bare_arg);
        a = node(p, OZNI_N_BareArg, start);
        set_text(p, a, name);
      }
      add(p, alt, finish(p, a));
      if (p->failed || !at(p, OZNI_T_COMMA)) {
        break;
      }
      advance(p);
    }
    add(p, ta, finish(p, alt));
    if (p->failed || !at(p, OZNI_T_PIPE)) {
      break;
    }
    advance(p);
  }
  expect(p, OZNI_T_GT);
  leave(p);
  add(p, parent, finish(p, ta));
  return p->failed ? 0 : ta;
}

static ozni_index type_ref(parser *p) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_type_ref);
  ozni_index n = node_here(p, OZNI_N_TypeRef);
  set_text(p, n, qual_name(p));
  if (at(p, OZNI_T_LPAREN) && !p->failed) {
    ozni_index ca = node_here(p, OZNI_N_CallArgs);
    call_args(p, ca);
    add(p, n, finish(p, ca));
  }
  if (at(p, OZNI_T_LT) && !p->failed) {
    type_args(p, n);
  }
  return p->failed ? 0 : finish(p, n);
}

static void type_list(parser *p, ozni_index parent) {
  mark(p, OZNI_R_type_list);
  for (;;) {
    add(p, parent, type_ref(p));
    if (p->failed || !at(p, OZNI_T_COMMA)) {
      return;
    }
    advance(p);
  }
}

static ozni_index type_field(parser *p);

static ozni_index type_atom(parser *p) {
  mark(p, OZNI_R_type_atom);
  if (at(p, OZNI_T_LBRACKET)) {
    mark(p, OZNI_R_list_type);
    if (!enter(p)) {
      return 0;
    }
    ozni_index n = node_here(p, OZNI_N_ListType);
    advance(p);
    add(p, n, type(p));
    expect(p, OZNI_T_RBRACKET);
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  if (at(p, OZNI_T_LBRACE)) {
    mark(p, OZNI_R_record_type);
    if (!enter(p)) {
      return 0;
    }
    ozni_index n = node_here(p, OZNI_N_RecordType);
    advance(p);
    skip_seps(p);
    while (!at(p, OZNI_T_RBRACE) && !p->failed) {
      add(p, n, type_field(p));
      if (!list_sep(p)) {
        break;
      }
    }
    skip_seps(p);
    expect(p, OZNI_T_RBRACE);
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  if (at(p, OZNI_T_LPAREN)) {
    mark(p, OZNI_R_tuple_type);
    if (!enter(p)) {
      return 0;
    }
    ozni_index n = node_here(p, OZNI_N_TupleType);
    advance(p);
    add(p, n, type(p));
    do {
      expect(p, OZNI_T_COMMA);
      add(p, n, type(p));
    } while (at(p, OZNI_T_COMMA) && !p->failed);
    expect(p, OZNI_T_RPAREN);
    leave(p);
    return p->failed ? 0 : finish(p, n);
  }
  return type_ref(p);
}

static ozni_index type_postfix(parser *p) {
  mark(p, OZNI_R_type_postfix);
  uint32_t start = cur(p)->start;
  ozni_index t = type_atom(p);
  if (t != 0 && at(p, OZNI_T_QUESTION)) {
    ozni_index n = node(p, OZNI_N_OptionalType, start);
    advance(p);
    add(p, n, t);
    return finish(p, n);
  }
  return t;
}

static ozni_index type(parser *p) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_type);
  mark(p, OZNI_R_type_union);
  uint32_t start = cur(p)->start;
  ozni_index t = type_postfix(p);
  if (t != 0 && at(p, OZNI_T_PIPE)) {
    ozni_index u = node(p, OZNI_N_UnionType, start);
    add(p, u, t);
    while (at(p, OZNI_T_PIPE) && !p->failed) {
      advance(p);
      add(p, u, type_postfix(p));
    }
    t = finish(p, u);
  }
  if (t != 0 && at_kw(p, "in")) {
    ozni_index r = node(p, OZNI_N_RangedType, start);
    advance(p);
    add(p, r, t);
    add(p, r, range(p));
    t = finish(p, r);
  }
  return p->failed ? 0 : t;
}

static ozni_index default_of(parser *p) {
  ozni_index d = node_here(p, OZNI_N_Default);
  add(p, d, expr(p));
  return finish(p, d);
}

static ozni_index type_field(parser *p) {
  mark(p, OZNI_R_type_field);
  mark(p, OZNI_R_word);
  if (!at(p, OZNI_T_IDENT) && !at(p, OZNI_T_KEYWORD)) {
    unexpected(p);
    return 0;
  }
  ozni_index n = node_here(p, OZNI_N_TypeField);
  set_text(p, n, cur(p)->text);
  advance(p);
  expect(p, OZNI_T_COLON);
  add(p, n, type(p));
  if (at(p, OZNI_T_EQ) && !p->failed) {
    advance(p);
    add(p, n, default_of(p));
  }
  annotations(p, n);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index param(parser *p) {
  mark(p, OZNI_R_param);
  ozni_index n = node_here(p, OZNI_N_Param);
  set_text(p, n, name_of(p, n));
  expect(p, OZNI_T_COLON);
  if (p->failed) {
    return 0;
  }
  if (is_amount_start(p, 0) || at(p, OZNI_T_STRING) || at(p, OZNI_T_MLSTRING) || at_kw(p, "true") ||
      at_kw(p, "false")) {
    ozni_index d = node_here(p, OZNI_N_Default);
    add(p, d, primary(p));
    add(p, n, finish(p, d));
  } else {
    add(p, n, type(p));
    if (at(p, OZNI_T_EQ) && !p->failed) {
      advance(p);
      add(p, n, default_of(p));
    }
  }
  return p->failed ? 0 : finish(p, n);
}

static void params(parser *p, ozni_index parent) {
  mark(p, OZNI_R_params);
  for (;;) {
    add(p, parent, param(p));
    if (p->failed || !at(p, OZNI_T_COMMA)) {
      return;
    }
    advance(p);
  }
}

static void paren_params(parser *p, ozni_index parent, bool required) {
  if (!at(p, OZNI_T_LPAREN)) {
    if (required) {
      unexpected(p);
    }
    return;
  }
  if (!enter(p)) {
    return;
  }
  advance(p);
  if (!at(p, OZNI_T_RPAREN)) {
    params(p, parent);
  }
  expect(p, OZNI_T_RPAREN);
  leave(p);
}

/* Statements */

static bool block_end(parser *p) {
  if (at(p, OZNI_T_SEP)) {
    return true;
  }
  if (at(p, OZNI_T_RBRACE)) {
    return true;
  }
  unexpected(p);
  return false;
}

static ozni_index stmt(parser *p, block_kind kind, bool top);

static ozni_index let_stmt(parser *p) {
  mark(p, OZNI_R_let);
  mark(p, OZNI_R_binding);
  uint32_t start = cur(p)->start;
  advance(p);
  ozni_index n;
  if (at(p, OZNI_T_LPAREN)) {
    mark(p, OZNI_R_tuple_binding);
    n = node(p, OZNI_N_LetTuple, start);
    advance(p);
    for (int k = 0; !p->failed; k++) {
      ozni_index b = node_here(p, OZNI_N_BindName);
      set_text(p, b, name_of(p, b));
      add(p, n, finish(p, b));
      if (!at(p, OZNI_T_COMMA)) {
        if (k == 0) {
          unexpected(p);
        }
        break;
      }
      advance(p);
    }
    expect(p, OZNI_T_RPAREN);
  } else if (at(p, OZNI_T_DOLLAR)) {
    n = node(p, OZNI_N_Let, start);
    uint32_t s0 = cur(p)->start;
    mark(p, OZNI_R_splice);
    advance(p);
    expect_ident(p);
    if (!p->failed) {
      ozni_index text = 0;
      put_text(p, p->c->srcs[p->file].text + s0, p->prev_end - s0, &text);
      set_text(p, n, text);
      if (p->quote_depth == 0) {
        diag_at(p, OZNI_D_SYN_spliceOutsideQuote, s0, p->prev_end, nullptr, 0);
      }
    }
  } else {
    n = node(p, OZNI_N_Let, start);
    set_text(p, n, name_of(p, n));
  }
  expect(p, OZNI_T_EQ);
  add(p, n, expr(p));
  annotations(p, n);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index if_stmt(parser *p) {
  mark(p, OZNI_R_if);
  ozni_index n = node_here(p, OZNI_N_If);
  advance(p);
  add(p, n, expr_no_construct(p));
  add(p, n, block(p, BLOCK_PLAIN));
  if (at_kw(p, "else") && !p->failed) {
    advance(p);
    if (at_kw(p, "if")) {
      if (enter(p)) {
        add(p, n, if_stmt(p));
        leave(p);
      }
    } else {
      add(p, n, block(p, BLOCK_PLAIN));
    }
  }
  return p->failed ? 0 : finish(p, n);
}

static ozni_index plate_stmt(parser *p) {
  mark(p, OZNI_R_plate);
  ozni_index n = node_here(p, OZNI_N_Plate);
  advance(p);
  set_text(p, n, name_of(p, n));
  if (at_word(p, "on") && !p->failed) {
    ozni_index on = node_here(p, OZNI_N_On);
    advance(p);
    add(p, on, type_ref(p));
    while (at_kw(p, "or") && !p->failed) {
      advance(p);
      add(p, on, type_ref(p));
    }
    add(p, n, finish(p, on));
  }
  items_into(p, n, OZNI_T_RBRACE);
  annotations(p, n);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index serve_stmt(parser *p) {
  mark(p, OZNI_R_serve);
  ozni_index n = node_here(p, OZNI_N_Serve);
  advance(p);
  set_text(p, n, name_of(p, n));
  if (at_word(p, "after") && !p->failed) {
    mark(p, OZNI_R_serve_after);
    ozni_index a = node_here(p, OZNI_N_After);
    advance(p);
    set_text(p, a, name_of(p, a));
    if (at(p, OZNI_T_PLUS) && !p->failed) {
      advance(p);
      add(p, a, sum(p));
    }
    add(p, n, finish(p, a));
  }
  if (expect(p, OZNI_T_LBRACE) && enter(p)) {
    skip_seps(p);
    while (at_word(p, "plate") && kind_at(p, 1) == OZNI_T_IDENT && !p->failed) {
      add(p, n, plate_stmt(p));
      skip_seps(p);
    }
    expect(p, OZNI_T_RBRACE);
    leave(p);
  }
  annotations(p, n);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index ingredients_stmt(parser *p) {
  mark(p, OZNI_R_ingredients);
  ozni_index n = node_here(p, OZNI_N_Ingredients);
  advance(p);
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !p->failed) {
    mark(p, OZNI_R_ingredient_item);
    ozni_index it = node_here(p, OZNI_N_Item);
    set_text(p, it, name_of(p, it));
    expect(p, OZNI_T_COLON);
    if (at(p, OZNI_T_IDENT) && !p->failed &&
        (is_amount_start(p, 1) || kind_at(p, 1) == OZNI_T_LT || kind_at(p, 1) == OZNI_T_LBRACKET)) {
      add(p, it, type_ref(p));
    }
    add(p, it, expr(p));
    annotations(p, it);
    add(p, n, finish(p, it));
    if (!list_sep(p)) {
      break;
    }
  }
  skip_seps(p);
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index stmt(parser *p, block_kind kind, bool top) {
  mark(p, OZNI_R_stmt);
  uint32_t start = cur(p)->start;
  if (at_kw(p, "let")) {
    return let_stmt(p);
  }
  if (at_kw(p, "parallel")) {
    mark(p, OZNI_R_parallel);
    ozni_index n = node_here(p, OZNI_N_Parallel);
    advance(p);
    if (expect(p, OZNI_T_LBRACE) && enter(p)) {
      skip_seps(p);
      while (at_kw(p, "branch") && !p->failed) {
        mark(p, OZNI_R_branch);
        ozni_index b = node_here(p, OZNI_N_Branch);
        advance(p);
        set_text(p, b, name_of(p, b));
        add(p, b, block(p, BLOCK_PLAIN));
        add(p, n, finish(p, b));
        skip_seps(p);
      }
      expect(p, OZNI_T_RBRACE);
      leave(p);
    }
    return p->failed ? 0 : finish(p, n);
  }
  if (at_kw(p, "if")) {
    return if_stmt(p);
  }
  if (at_kw(p, "for")) {
    mark(p, OZNI_R_for);
    ozni_index n = node_here(p, OZNI_N_ForIn);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (!p->failed && !at_kw(p, "in")) {
      unexpected(p);
    }
    advance(p);
    add(p, n, expr_no_construct(p));
    add(p, n, block(p, BLOCK_PLAIN));
    return p->failed ? 0 : finish(p, n);
  }
  if (at_kw(p, "repeat")) {
    mark(p, OZNI_R_repeat);
    ozni_index n = node_here(p, OZNI_N_Repeat);
    advance(p);
    add(p, n, expr_no_construct(p));
    add(p, n, block(p, BLOCK_PLAIN));
    return p->failed ? 0 : finish(p, n);
  }
  if (at_kw(p, "serve") || at_kw(p, "ingredients")) {
    bool serve = at_kw(p, "serve");
    ozni_index n = serve ? serve_stmt(p) : ingredients_stmt(p);
    bool allowed = top && (serve ? (kind == BLOCK_RECIPE || kind == BLOCK_MENU) : kind == BLOCK_RECIPE);
    if (n != 0 && !allowed) {
      ozni_str arg = serve ? (ozni_str){"serve", 5} : (ozni_str){"ingredients", 11};
      diag_at(p, OZNI_D_SYN_memberPlacement, start, p->prev_end, &arg, 1);
    }
    return n;
  }
  mark(p, OZNI_R_expr_stmt);
  ozni_index e = expr(p);
  annotations(p, e);
  return p->failed ? 0 : e;
}

static ozni_index block(parser *p, block_kind kind) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_block);
  ozni_index n = node_here(p, OZNI_N_Block);
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  int saved_quote = p->quote_depth;
  if (p->quote_depth == -1) {
    p->quote_depth = 0;
  }
  skip_seps(p);
  bool top_quote = kind == BLOCK_MACRO;
  while (!at(p, OZNI_T_RBRACE) && !at(p, OZNI_T_EOF) && !p->oom) {
    if (top_quote) {
      p->quote_depth = at_kw(p, "quote") ? -1 : 0;
    }
    ozni_index s = stmt(p, kind, true);
    if (top_quote) {
      p->quote_depth = 0;
    }
    if (s != 0 && block_end(p)) {
      add(p, n, s);
    }
    if (p->failed) {
      sync(p);
      p->failed = false;
    }
    skip_seps(p);
  }
  p->quote_depth = saved_quote;
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : finish(p, n);
}

/* Declarations */

static ozni_index member(parser *p, body_kind kind) {
  mark(p, OZNI_R_member);
  uint32_t start = cur(p)->start;
  bool action_only = false;
  ozni_index n = 0;
  const char *word = nullptr;
  if (at(p, OZNI_T_KEYWORD) && kind_at(p, 1) != OZNI_T_COLON &&
      (at_kw(p, "uses") || at_kw(p, "takes") || at_kw(p, "variant") || at_kw(p, "steps"))) {
    action_only = true;
    if (at_kw(p, "uses")) {
      word = "uses";
      mark(p, OZNI_R_uses);
      n = node_here(p, OZNI_N_Uses);
      advance(p);
      type_list(p, n);
      if (at_kw(p, "for") && !p->failed) {
        ozni_index f = node_here(p, OZNI_N_For);
        advance(p);
        add(p, f, expr(p));
        add(p, n, finish(p, f));
      }
      annotations(p, n);
    } else if (at_kw(p, "takes")) {
      word = "takes";
      mark(p, OZNI_R_takes);
      n = node_here(p, OZNI_N_Takes);
      advance(p);
      add(p, n, expr(p));
      annotations(p, n);
    } else if (at_kw(p, "variant")) {
      word = "variant";
      mark(p, OZNI_R_variant);
      n = node_here(p, OZNI_N_Variant);
      advance(p);
      set_text(p, n, name_of(p, n));
      ozni_index body = decl_body(p, BODY_VARIANT);
      if (body != 0) {
        for (ozni_index c = N(p, body)->first; c != 0; c = N(p, c)->next) {
          add(p, n, c);
        }
      }
      annotations(p, n);
      if (kind == BODY_VARIANT) {
        action_only = false;
        if (!p->failed) {
          ozni_str arg = {"variant", 7};
          diag_at(p, OZNI_D_SYN_memberPlacement, start, p->prev_end, &arg, 1);
        }
        return 0;
      }
    } else {
      word = "steps";
      mark(p, OZNI_R_steps);
      n = node_here(p, OZNI_N_Steps);
      advance(p);
      add(p, n, block(p, BLOCK_PLAIN));
    }
  } else if ((at(p, OZNI_T_IDENT) || at(p, OZNI_T_KEYWORD)) && kind_at(p, 1) == OZNI_T_COLON) {
    mark(p, OZNI_R_field);
    mark(p, OZNI_R_word);
    n = node_here(p, OZNI_N_Field);
    set_text(p, n, cur(p)->text);
    advance(p);
    advance(p);
    for (;;) {
      mark(p, OZNI_R_field_item);
      uint32_t s = cur(p)->start;
      ozni_index v = expr(p);
      if (v != 0 && at_word(p, "on")) {
        ozni_index on = node(p, OZNI_N_On, s);
        advance(p);
        add(p, on, v);
        add(p, on, type_ref(p));
        v = finish(p, on);
      }
      add(p, n, v);
      if (p->failed || !at(p, OZNI_T_COMMA) ||
          ((kind_at(p, 1) == OZNI_T_IDENT || kind_at(p, 1) == OZNI_T_KEYWORD) && kind_at(p, 2) == OZNI_T_COLON)) {
        break;
      }
      advance(p);
    }
    annotations(p, n);
  } else if (at(p, OZNI_T_IDENT) && kind_at(p, 1) == OZNI_T_LBRACE) {
    mark(p, OZNI_R_section);
    n = node_here(p, OZNI_N_Section);
    set_text(p, n, cur(p)->text);
    advance(p);
    ozni_index body = decl_body(p, BODY_PLAIN);
    if (body != 0) {
      for (ozni_index c = N(p, body)->first; c != 0; c = N(p, c)->next) {
        add(p, n, c);
      }
    }
  } else if (at(p, OZNI_T_IDENT)) {
    mark(p, OZNI_R_marker);
    n = node_here(p, OZNI_N_Marker);
    set_text(p, n, cur(p)->text);
    advance(p);
  } else {
    unexpected(p);
    return 0;
  }
  if (p->failed) {
    return 0;
  }
  finish(p, n);
  if (action_only && kind == BODY_PLAIN) {
    ozni_str arg = {word, (uint32_t)strlen(word)};
    diag_at(p, OZNI_D_SYN_memberPlacement, start, p->prev_end, &arg, 1);
    return 0;
  }
  return n;
}

static ozni_index decl_body(parser *p, body_kind kind) {
  if (p->failed) {
    return 0;
  }
  mark(p, OZNI_R_decl_body);
  ozni_index n = node_here(p, OZNI_N_Block);
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !at(p, OZNI_T_EOF) && !p->oom) {
    ozni_index m = member(p, kind);
    if (m != 0 && (at(p, OZNI_T_COMMA) || block_end(p))) {
      add(p, n, m);
    }
    if (p->failed) {
      sync(p);
      p->failed = false;
    } else if (at(p, OZNI_T_COMMA)) {
      list_sep(p);
    }
    skip_seps(p);
  }
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : finish(p, n);
}

static void body_into(parser *p, ozni_index n, body_kind kind) {
  ozni_index body = decl_body(p, kind);
  if (body != 0) {
    for (ozni_index c = N(p, body)->first; c != 0; c = N(p, c)->next) {
      add(p, n, c);
    }
  }
}

static ozni_index value_set(parser *p, ozni_index parent) {
  mark(p, OZNI_R_value_set);
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !p->failed) {
    mark(p, OZNI_R_value_item);
    if (at_kw(p, "finer")) {
      mark(p, OZNI_R_finer);
      ozni_index f = node_here(p, OZNI_N_Finer);
      advance(p);
      set_text(p, f, name_of(p, f));
      expect(p, OZNI_T_ARROW);
      for (;;) {
        ozni_index nm = node_here(p, OZNI_N_Name);
        set_text(p, nm, name_of(p, nm));
        add(p, f, finish(p, nm));
        if (p->failed || !at(p, OZNI_T_COMMA)) {
          break;
        }
        advance(p);
      }
      add(p, parent, finish(p, f));
    } else {
      mark(p, OZNI_R_enum_value);
      ozni_index v = node_here(p, OZNI_N_Value);
      set_text(p, v, name_of(p, v));
      if (at(p, OZNI_T_LPAREN)) {
        paren_params(p, v, true);
      }
      add(p, parent, finish(p, v));
    }
    if (!list_sep(p)) {
      break;
    }
  }
  skip_seps(p);
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : parent;
}

static ozni_index simple_decl(parser *p, ozni_node_kind kind, ozni_rule rule, bool allow_params) {
  mark(p, rule);
  ozni_index n = node_here(p, kind);
  advance(p);
  set_text(p, n, name_of(p, n));
  if (allow_params && at(p, OZNI_T_LPAREN)) {
    paren_params(p, n, true);
  }
  if (at(p, OZNI_T_COLON) && !p->failed) {
    advance(p);
    type_list(p, n);
  }
  annotations(p, n);
  if (at(p, OZNI_T_LBRACE) && !p->failed) {
    body_into(p, n, BODY_PLAIN);
  }
  return p->failed ? 0 : finish(p, n);
}

static ozni_index action_decl(parser *p) {
  mark(p, OZNI_R_action);
  ozni_index n = node_here(p, OZNI_N_Action);
  advance(p);
  set_text(p, n, name_of(p, n));
  paren_params(p, n, true);
  if (at(p, OZNI_T_ARROW) && !p->failed) {
    ozni_index o = node_here(p, OZNI_N_Output);
    advance(p);
    add(p, o, type(p));
    add(p, n, finish(p, o));
  }
  if (at_kw(p, "where") && !p->failed) {
    mark(p, OZNI_R_where);
    advance(p);
    for (;;) {
      mark(p, OZNI_R_where_item);
      ozni_index w = node_here(p, OZNI_N_Where);
      set_text(p, w, name_of(p, w));
      expect(p, OZNI_T_COLON);
      add(p, w, type_ref(p));
      add(p, n, finish(p, w));
      if (p->failed || !at(p, OZNI_T_COMMA)) {
        break;
      }
      advance(p);
    }
  }
  annotations(p, n);
  if (at(p, OZNI_T_LBRACE) && !p->failed) {
    body_into(p, n, BODY_ACTION);
  }
  return p->failed ? 0 : finish(p, n);
}

static ozni_index recipe_or_menu(parser *p, bool recipe) {
  mark(p, recipe ? OZNI_R_recipe : OZNI_R_menu);
  ozni_index n = node_here(p, recipe ? OZNI_N_Recipe : OZNI_N_Menu);
  advance(p);
  set_text(p, n, name_of(p, n));
  paren_params(p, n, false);
  if (recipe && at(p, OZNI_T_ARROW) && !p->failed) {
    ozni_index o = node_here(p, OZNI_N_Output);
    advance(p);
    add(p, o, type(p));
    add(p, n, finish(p, o));
  }
  if (recipe && at_kw(p, "requires") && !p->failed) {
    mark(p, OZNI_R_requires);
    ozni_index r = node_here(p, OZNI_N_Requires);
    advance(p);
    type_list(p, r);
    add(p, n, finish(p, r));
  }
  annotations(p, n);
  add(p, n, block(p, recipe ? BLOCK_RECIPE : BLOCK_MENU));
  return p->failed ? 0 : finish(p, n);
}

static ozni_index type_decl(parser *p) {
  mark(p, OZNI_R_type_decl);
  ozni_index n = node_here(p, OZNI_N_Type);
  advance(p);
  set_text(p, n, name_of(p, n));
  if (at(p, OZNI_T_COLON) && !p->failed) {
    advance(p);
    type_list(p, n);
  }
  annotations(p, n);
  mark(p, OZNI_R_type_body);
  if (!expect(p, OZNI_T_LBRACE) || !enter(p)) {
    return 0;
  }
  skip_seps(p);
  while (!at(p, OZNI_T_RBRACE) && !at(p, OZNI_T_EOF) && !p->oom) {
    mark(p, OZNI_R_type_member);
    ozni_index m;
    if (at_kw(p, "check")) {
      mark(p, OZNI_R_check);
      m = node_here(p, OZNI_N_Check);
      advance(p);
      ozni_index first = expect_ident(p);
      if (at(p, OZNI_T_IDENT)) {
        if (m != 0) {
          N(p, m)->text2 = first;
        }
        first = expect_ident(p);
      }
      set_text(p, m, first);
      expect(p, OZNI_T_COLON);
      add(p, m, expr(p));
      if (at_kw(p, "else") && !p->failed) {
        advance(p);
        ozni_index sev = expect_ident(p);
        if (m != 0) {
          N(p, m)->text3 = sev;
        }
      }
      annotations(p, m);
      m = p->failed ? 0 : finish(p, m);
    } else {
      m = type_field(p);
    }
    if (m != 0 && (at(p, OZNI_T_COMMA) || block_end(p))) {
      add(p, n, m);
    }
    if (p->failed) {
      sync(p);
      p->failed = false;
    } else if (at(p, OZNI_T_COMMA)) {
      list_sep(p);
    }
    skip_seps(p);
  }
  expect(p, OZNI_T_RBRACE);
  leave(p);
  return p->failed ? 0 : finish(p, n);
}

static ozni_index decl(parser *p) {
  mark(p, OZNI_R_decl);
  const ozni_token *t = cur(p);
  static const struct {
    const char *word;
    ozni_node_kind kind;
    ozni_rule rule;
    bool params;
  } simple[] = {
      {"trait", OZNI_N_Trait, OZNI_R_trait, true},          {"ingredient", OZNI_N_Ingredient, OZNI_R_ingredient, false},
      {"kind", OZNI_N_Kind, OZNI_R_kind, false},           {"vessel", OZNI_N_Vessel, OZNI_R_vessel, false},
      {"dirt", OZNI_N_Dirt, OZNI_R_dirt, false},           {"resource", OZNI_N_Resource, OZNI_R_resource, false},
      {"tool", OZNI_N_Tool, OZNI_R_tool, false},
  };
  for (size_t k = 0; k < sizeof simple / sizeof simple[0]; k++) {
    if (text_is(p, t, simple[k].word)) {
      return simple_decl(p, simple[k].kind, simple[k].rule, simple[k].params);
    }
  }
  if (text_is(p, t, "dimension")) {
    mark(p, OZNI_R_dimension);
    ozni_index n = node_here(p, OZNI_N_Dimension);
    advance(p);
    for (;;) {
      ozni_index nm = node_here(p, OZNI_N_Name);
      set_text(p, nm, name_of(p, nm));
      add(p, n, finish(p, nm));
      if (p->failed || !at(p, OZNI_T_COMMA)) {
        break;
      }
      advance(p);
    }
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "unit")) {
    mark(p, OZNI_R_unit);
    ozni_index n = node_here(p, OZNI_N_Unit);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (at(p, OZNI_T_COLON) && !p->failed) {
      advance(p);
      add(p, n, type_ref(p));
    }
    expect(p, OZNI_T_EQ);
    if (at_word(p, "base") && !p->failed) {
      ozni_index b = node_here(p, OZNI_N_Base);
      advance(p);
      add(p, n, finish(p, b));
    } else {
      add(p, n, expr(p));
    }
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "axis") || text_is(p, t, "enum")) {
    bool axis = text_is(p, t, "axis");
    mark(p, axis ? OZNI_R_axis : OZNI_R_enum);
    ozni_index n = node_here(p, axis ? OZNI_N_Axis : OZNI_N_Enum);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (axis) {
      expect(p, OZNI_T_COLON);
    }
    if (!p->failed && (axis || at(p, OZNI_T_COLON))) {
      if (!axis) {
        advance(p);
      }
      if (at_word(p, "ordered") || (axis && at_word(p, "set"))) {
        if (n != 0) {
          N(p, n)->text2 = cur(p)->text;
        }
        advance(p);
      } else if (!axis) {
        unexpected(p);
      }
    }
    value_set(p, n);
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "alias")) {
    mark(p, OZNI_R_alias);
    ozni_index n = node_here(p, OZNI_N_Alias);
    advance(p);
    set_text(p, n, name_of(p, n));
    expect(p, OZNI_T_EQ);
    add(p, n, type(p));
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "rule")) {
    mark(p, OZNI_R_rule);
    ozni_index n = node_here(p, OZNI_N_Rule);
    advance(p);
    if (at(p, OZNI_T_IDENT) && kind_at(p, 1) == OZNI_T_IDENT && text_is(p, tok_at(p, 1), "after")) {
      set_text(p, n, name_of(p, n));
    }
    if (!at_word(p, "after")) {
      unexpected(p);
      return 0;
    }
    advance(p);
    add(p, n, type_ref(p));
    expect(p, OZNI_T_COLON);
    for (;;) {
      mark(p, OZNI_R_rule_case);
      ozni_index rc = node_here(p, OZNI_N_RuleCase);
      set_text(p, rc, name_of(p, rc));
      if (at_word(p, "before") && !p->failed) {
        mark(p, OZNI_R_rule_target);
        advance(p);
        if (at_word(p, "any") && kind_at(p, 1) == OZNI_T_IDENT && text_is(p, tok_at(p, 1), "use")) {
          ozni_index a = node_here(p, OZNI_N_AnyUse);
          advance(p);
          advance(p);
          add(p, rc, finish(p, a));
        } else {
          add(p, rc, type_ref(p));
        }
      }
      add(p, n, finish(p, rc));
      if (p->failed || !at(p, OZNI_T_COMMA)) {
        break;
      }
      advance(p);
    }
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "property")) {
    mark(p, OZNI_R_property);
    ozni_index n = node_here(p, OZNI_N_Property);
    advance(p);
    set_text(p, n, name_of(p, n));
    expect(p, OZNI_T_EQ);
    add(p, n, expr(p));
    annotations(p, n);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "source")) {
    mark(p, OZNI_R_source);
    ozni_index n = node_here(p, OZNI_N_Source);
    advance(p);
    set_text(p, n, name_of(p, n));
    body_into(p, n, BODY_PLAIN);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "action")) {
    return action_decl(p);
  }
  if (text_is(p, t, "extend")) {
    mark(p, OZNI_R_extend);
    ozni_index n = node_here(p, OZNI_N_Extend);
    advance(p);
    if (!at_word(p, "action")) {
      unexpected(p);
      return 0;
    }
    advance(p);
    set_text(p, n, qual_name(p));
    body_into(p, n, BODY_ACTION);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "recipe") || text_is(p, t, "menu")) {
    return recipe_or_menu(p, text_is(p, t, "recipe"));
  }
  if (text_is(p, t, "kitchen")) {
    mark(p, OZNI_R_kitchen);
    ozni_index n = node_here(p, OZNI_N_Kitchen);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (at(p, OZNI_T_COLON) && !p->failed) {
      advance(p);
      add(p, n, type_ref(p));
    }
    annotations(p, n);
    body_into(p, n, BODY_PLAIN);
    return p->failed ? 0 : finish(p, n);
  }
  if (text_is(p, t, "type")) {
    return type_decl(p);
  }
  unexpected(p);
  return 0;
}

static bool is_decl_word(const parser *p) {
  static const char *const words[] = {"dimension", "unit",   "axis",     "trait",  "ingredient", "alias",
                                      "kind",      "vessel", "dirt",     "rule",   "resource",   "tool",
                                      "action",    "extend", "recipe",   "menu",   "kitchen",    "enum",
                                      "type",      "property", "source"};
  if (!at(p, OZNI_T_IDENT)) {
    return false;
  }
  for (size_t k = 0; k < sizeof words / sizeof words[0]; k++) {
    if (text_is(p, cur(p), words[k])) {
      return true;
    }
  }
  return false;
}

static ozni_index macro_decl(parser *p) {
  mark(p, OZNI_R_macro);
  ozni_index n = node_here(p, OZNI_N_Macro);
  advance(p);
  set_text(p, n, name_of(p, n));
  if (expect(p, OZNI_T_LPAREN)) {
    if (!at(p, OZNI_T_RPAREN)) {
      for (;;) {
        ozni_index mp = node_here(p, OZNI_N_MacroParam);
        set_text(p, mp, name_of(p, mp));
        add(p, n, finish(p, mp));
        if (p->failed || !at(p, OZNI_T_COMMA)) {
          break;
        }
        advance(p);
      }
    }
    expect(p, OZNI_T_RPAREN);
  }
  ozni_index b = block(p, BLOCK_MACRO);
  if (b != 0) {
    ozni_node *bn = N(p, b);
    bool ok = bn->first != 0 && bn->first == bn->last && N(p, bn->first)->kind == OZNI_N_Quote;
    if (!ok) {
      diag_at(p, OZNI_D_SYN_macroBody, bn->span.start, bn->span.end, nullptr, 0);
    }
  }
  add(p, n, b);
  return p->failed ? 0 : finish(p, n);
}

static bool item(parser *p, ozni_index parent) {
  mark(p, OZNI_R_item);
  ozni_index n = 0;
  if (at_kw(p, "local")) {
    mark(p, OZNI_R_local_decl);
    n = node_here(p, OZNI_N_Local);
    advance(p);
    if (is_decl_word(p)) {
      add(p, n, decl(p));
    } else if (at_kw(p, "macro")) {
      add(p, n, macro_decl(p));
    } else {
      unexpected(p);
    }
    n = p->failed ? 0 : finish(p, n);
  } else if (is_decl_word(p)) {
    n = decl(p);
  } else if (at_kw(p, "macro")) {
    mark(p, OZNI_R_decl);
    n = macro_decl(p);
  } else if (at_kw(p, "import")) {
    mark(p, OZNI_R_import);
    n = node_here(p, OZNI_N_Import);
    advance(p);
    uint32_t start = cur(p)->start;
    const char *s = p->c->srcs[p->file].text;
    if (!at(p, OZNI_T_IDENT)) {
      unexpected(p);
    } else {
      mark(p, OZNI_R_qual_name);
      advance(p);
      while (at(p, OZNI_T_DOT) && kind_at(p, 1) == OZNI_T_IDENT) {
        advance(p);
        advance(p);
      }
      ozni_index text = 0;
      put_text(p, s + start, p->prev_end - start, &text);
      set_text(p, n, text);
      if (at(p, OZNI_T_DOT) && kind_at(p, 1) == OZNI_T_LBRACE) {
        mark(p, OZNI_R_import_names);
        advance(p);
        advance(p);
        for (;;) {
          ozni_index in = node_here(p, OZNI_N_ImportName);
          set_text(p, in, name_of(p, in));
          add(p, n, finish(p, in));
          if (p->failed || !at(p, OZNI_T_COMMA)) {
            break;
          }
          advance(p);
        }
        expect(p, OZNI_T_RBRACE);
      } else if (at(p, OZNI_T_DOT) && kind_at(p, 1) == OZNI_T_STAR) {
        diag_at(p, OZNI_D_SYN_importStar, cur(p)->start, tok_at(p, 1)->end, nullptr, 0);
        advance(p);
        advance(p);
        p->failed = true;
      } else if (at_word(p, "as")) {
        mark(p, OZNI_R_import_alias);
        ozni_index al = node_here(p, OZNI_N_ImportAlias);
        advance(p);
        set_text(p, al, name_of(p, al));
        add(p, n, finish(p, al));
      }
    }
    n = p->failed ? 0 : finish(p, n);
  } else if (at_kw(p, "module")) {
    mark(p, OZNI_R_module);
    n = node_here(p, OZNI_N_Module);
    advance(p);
    set_text(p, n, qual_name(p));
    if (expect(p, OZNI_T_LBRACE) && enter(p)) {
      skip_seps(p);
      while (!at(p, OZNI_T_RBRACE) && !at(p, OZNI_T_EOF) && !p->oom) {
        if (item(p, n) && !at(p, OZNI_T_SEP) && !at(p, OZNI_T_RBRACE)) {
          unexpected(p);
        }
        if (p->failed) {
          sync(p);
          p->failed = false;
        }
        skip_seps(p);
      }
      expect(p, OZNI_T_RBRACE);
      leave(p);
    }
    n = p->failed ? 0 : finish(p, n);
  } else if (at_kw(p, "package")) {
    mark(p, OZNI_R_package);
    n = node_here(p, OZNI_N_Package);
    advance(p);
    set_text(p, n, name_of(p, n));
    body_into(p, n, BODY_PLAIN);
    n = p->failed ? 0 : finish(p, n);
  } else if (at_kw(p, "phrases")) {
    mark(p, OZNI_R_phrases);
    n = node_here(p, OZNI_N_Phrases);
    advance(p);
    set_text(p, n, name_of(p, n));
    if (!p->failed && (at(p, OZNI_T_STRING) || at(p, OZNI_T_MLSTRING))) {
      add(p, n, string_node(p));
    } else {
      unexpected(p);
    }
    n = p->failed ? 0 : finish(p, n);
  } else if (at_word(p, "bundle") && kind_at(p, 1) == OZNI_T_LBRACE) {
    mark(p, OZNI_R_bundle);
    n = node_here(p, OZNI_N_Bundle);
    advance(p);
    advance(p);
    if (enter(p)) {
      skip_seps(p);
      while (!at(p, OZNI_T_RBRACE) && !at(p, OZNI_T_EOF) && !p->failed) {
        mark(p, OZNI_R_bundle_member);
        if (at_word(p, "from") && kind_at(p, 1) == OZNI_T_COLON) {
          mark(p, OZNI_R_bundle_from);
          ozni_index f = node_here(p, OZNI_N_BundleFrom);
          advance(p);
          advance(p);
          set_text(p, f, qual_name(p));
          if (!p->failed && at(p, OZNI_T_STRING)) {
            if (f != 0) {
              N(p, f)->text2 = cur(p)->text;
            }
            advance(p);
          } else {
            unexpected(p);
          }
          add(p, n, finish(p, f));
        } else {
          add(p, n, member(p, BODY_PLAIN));
        }
        if (!list_sep(p)) {
          break;
        }
      }
      skip_seps(p);
      expect(p, OZNI_T_RBRACE);
      leave(p);
    }
    n = p->failed ? 0 : finish(p, n);
  } else {
    unexpected(p);
    return false;
  }
  add(p, parent, n);
  return n != 0;
}

bool ozni_parse(ozni_ctx *c, ozni_index file, ozni_ast *ast) {
  memset(ast->rules, 0, sizeof ast->rules);
  ast->nodes = ozni_table_new(c->arena, sizeof(ozni_node));
  ozni_table *toks = ozni_table_new(c->arena, sizeof(ozni_token));
  if (ast->nodes == nullptr || toks == nullptr) {
    return false;
  }
  ozni_node none = {0};
  if (!ozni_table_push(ast->nodes, &none, nullptr) || !ozni_lex(c, file, toks)) {
    return false;
  }
  parser p = {.c = c, .file = file, .ast = ast, .toks = toks};
  mark(&p, OZNI_R_file);
  ast->root = node(&p, OZNI_N_File, 0);
  if (ast->root == 0) {
    return false;
  }
  ozni_index path = 0;
  const char *pth = c->srcs[file].path;
  if (!put_text(&p, pth, strlen(pth), &path)) {
    return false;
  }
  N(&p, ast->root)->text = path;
  skip_seps(&p);
  while (!at(&p, OZNI_T_EOF) && !p.oom) {
    if (item(&p, ast->root) && !at(&p, OZNI_T_SEP) && !at(&p, OZNI_T_EOF)) {
      unexpected(&p);
    }
    if (p.failed) {
      sync(&p);
      if (at(&p, OZNI_T_RBRACE)) {
        advance(&p);
      }
      p.failed = false;
    }
    skip_seps(&p);
  }
  N(&p, ast->root)->span.end = (uint32_t)c->srcs[file].len;
  return !p.oom;
}

const ozni_node *ozni_node_get(const ozni_ast *ast, ozni_index i) {
  return i == 0 ? nullptr : ozni_table_get(ast->nodes, i);
}

const char *ozni_node_kind_name(ozni_node_kind k) {
  static const char *const names[] = {
#define OZNI_NODE_NAME(n) #n,
      OZNI_NODE_KINDS(OZNI_NODE_NAME)
#undef OZNI_NODE_NAME
  };
  if (k == OZNI_N_ForIn) {
    return "For";
  }
  return k < OZNI_N_COUNT ? names[k] : "?";
}

const char *ozni_rule_name(ozni_rule r) {
  static const char *const names[] = {
#define OZNI_RULE(n) #n,
#include "rules.def"
#undef OZNI_RULE
  };
  return r < OZNI_R_COUNT ? names[r] : "?";
}
