/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "core.h"

typedef enum tk : uint8_t {
  T_EOF,
  T_NEWLINE,
  T_IDENT,
  T_NUMBER,
  T_STRING,
  T_LPAREN,
  T_RPAREN,
  T_LBRACE,
  T_RBRACE,
  T_LBRACKET,
  T_RBRACKET,
  T_COMMA,
  T_SEMI,
  T_COLON,
  T_ASSIGN,
  T_EQ,
  T_NE,
  T_LT,
  T_LE,
  T_GT,
  T_GE,
  T_PLUS,
  T_MINUS,
  T_STAR,
  T_SLASH,
  T_DOTDOT,
  T_AT,
  T_DOLLAR,
  T_BAD,
  T_ACTION,
  T_MACRO,
  T_RECIPE,
  T_INGREDIENTS,
  T_LET,
  T_IF,
  T_ELSE,
  T_FOR,
  T_IN,
  T_REPEAT,
  T_QUOTE,
  T_AND,
  T_OR,
  T_NOT,
  T_TRUE,
  T_FALSE
} tk;

static const char *const keywords[] = {"action", "macro", "recipe", "ingredients", "let", "if", "else", "for",
                                       "in", "repeat", "quote", "and", "or", "not", "true", "false"};

typedef struct token {
  tk kind;
  bool numerr;
  ozpi_span span;
  ozpi_str text;
  ozpi_str str;
  ozpi_rat num;
} token;

typedef struct lexstate {
  size_t i;
  uint32_t line, col;
  uint32_t nbr;
} lexstate;

typedef struct parser {
  ozpi_ctx *c;
  const char *src;
  size_t len;
  lexstate ls;
  char *br;
  uint32_t capbr;
  token tok;
  ozpi_pos prev_end;
  uint32_t depth;
  bool abort;
  bool quiet;
  bool in_quote;
} parser;

static bool is_kw(tk k) { return k >= T_ACTION; }

static ozpi_pos here(const parser *p) { return (ozpi_pos){p->ls.line, p->ls.col}; }

static void advance_byte(parser *p) {
  if (p->src[p->ls.i] == '\n') {
    p->ls.line++;
    p->ls.col = 1;
  } else {
    p->ls.col++;
  }
  p->ls.i++;
}

static bool push_br(parser *p, char ch) {
  if (!ozpi_grow(p->c, (void **)&p->br, &p->capbr, 1, p->ls.nbr + 1)) {
    return false;
  }
  p->br[p->ls.nbr++] = ch;
  return true;
}

static char top_br(const parser *p) { return p->ls.nbr == 0 ? '{' : p->br[p->ls.nbr - 1]; }

static void lex_diag1(parser *p, ozpi_diag_id id, ozpi_span sp, ozpi_str a) {
  if (!p->quiet) {
    ozpi_diag1(p->c, id, sp, 0, a);
  }
}

static bool ident_start(unsigned char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch >= 0x80;
}

static bool ident_char(unsigned char ch) { return ident_start(ch) || (ch >= '0' && ch <= '9'); }

static void lex_number(parser *p, token *t) {
  size_t start = p->ls.i;
  ozpi_u128 acc = ozpi_u128_from(0);
  ozpi_u128 den = ozpi_u128_from(1);
  bool over = false;
  const ozpi_u128 max = ozpi_u128_from((uint64_t)INT64_MAX);
  while (p->ls.i < p->len && p->src[p->ls.i] >= '0' && p->src[p->ls.i] <= '9') {
    if (!over) {
      ozpi_u128 ten = ozpi_u128_mul64(acc.lo, 10);
      acc = ozpi_u128_add(ten, ozpi_u128_from((uint64_t)(p->src[p->ls.i] - '0')));
      over = ozpi_u128_cmp(acc, max) > 0;
    }
    advance_byte(p);
  }
  if (p->ls.i + 1 < p->len && p->src[p->ls.i] == '.' && p->src[p->ls.i + 1] >= '0' && p->src[p->ls.i + 1] <= '9') {
    advance_byte(p);
    while (p->ls.i < p->len && p->src[p->ls.i] >= '0' && p->src[p->ls.i] <= '9') {
      if (!over) {
        acc = ozpi_u128_add(ozpi_u128_mul64(acc.lo, 10), ozpi_u128_from((uint64_t)(p->src[p->ls.i] - '0')));
        den = ozpi_u128_mul64(den.lo, 10);
        over = ozpi_u128_cmp(acc, max) > 0 || ozpi_u128_cmp(den, max) > 0;
      }
      advance_byte(p);
    }
  }
  t->kind = T_NUMBER;
  t->text = (ozpi_str){p->src + start, (uint32_t)(p->ls.i - start)};
  t->num = ozpi_rat_int(0);
  if (over || ozpi_rat_from_i128((ozpi_i128){false, acc}, den, &t->num) != OZPI_OK) {
    t->numerr = true;
  }
}

static void lex_string(parser *p, token *t) {
  ozpi_buf b = {0};
  advance_byte(p);
  for (;;) {
    if (p->ls.i >= p->len) {
      ozpi_pos at = here(p);
      lex_diag1(p, OZPI_D_SYN_unexpectedToken, (ozpi_span){at, at}, ozpi_strz("end of input"));
      break;
    }
    char ch = p->src[p->ls.i];
    if (ch == '"') {
      advance_byte(p);
      break;
    }
    if (ch == '\\') {
      ozpi_pos s = here(p);
      char next = p->ls.i + 1 < p->len ? p->src[p->ls.i + 1] : '\0';
      if (next == '"' || next == '\\' || next == 'n') {
        ozpi_buf_put(p->c, &b, next == 'n' ? "\n" : &next, 1);
        advance_byte(p);
        advance_byte(p);
        continue;
      }
      advance_byte(p);
      size_t n = p->ls.i < p->len ? 1 : 0;
      if (n == 1) {
        advance_byte(p);
      }
      lex_diag1(p, OZPI_D_SYN_badEscape, (ozpi_span){s, here(p)}, (ozpi_str){p->src + p->ls.i - 1 - n, (uint32_t)(1 + n)});
      continue;
    }
    ozpi_buf_put(p->c, &b, &ch, 1);
    advance_byte(p);
  }
  t->kind = T_STRING;
  t->str = ozpi_buf_str(&b);
}

static void lex(parser *p, token *t) {
  *t = (token){0};
  for (;;) {
    if (p->ls.i >= p->len) {
      t->kind = T_EOF;
      t->span = (ozpi_span){here(p), here(p)};
      return;
    }
    char ch = p->src[p->ls.i];
    if (ch == ' ' || ch == '\t' || ch == '\r') {
      advance_byte(p);
      continue;
    }
    if (ch == '\n') {
      char top = top_br(p);
      if (top == '(' || top == '[') {
        advance_byte(p);
        continue;
      }
      ozpi_pos s = here(p);
      advance_byte(p);
      t->kind = T_NEWLINE;
      t->span = (ozpi_span){s, (ozpi_pos){s.line, s.col + 1}};
      t->text = ozpi_strz("newline");
      return;
    }
    if (ch == '/' && p->ls.i + 1 < p->len && p->src[p->ls.i + 1] == '/') {
      while (p->ls.i < p->len && p->src[p->ls.i] != '\n') {
        advance_byte(p);
      }
      continue;
    }
    if (ch == '/' && p->ls.i + 1 < p->len && p->src[p->ls.i + 1] == '*') {
      advance_byte(p);
      advance_byte(p);
      while (p->ls.i < p->len && !(p->src[p->ls.i] == '*' && p->ls.i + 1 < p->len && p->src[p->ls.i + 1] == '/')) {
        advance_byte(p);
      }
      if (p->ls.i >= p->len) {
        ozpi_pos at = here(p);
        lex_diag1(p, OZPI_D_SYN_unexpectedToken, (ozpi_span){at, at}, ozpi_strz("end of input"));
        continue;
      }
      advance_byte(p);
      advance_byte(p);
      continue;
    }
    break;
  }
  ozpi_pos s = here(p);
  size_t start = p->ls.i;
  unsigned char ch = (unsigned char)p->src[p->ls.i];
  if (ch >= '0' && ch <= '9') {
    lex_number(p, t);
  } else if (ch == '"') {
    lex_string(p, t);
  } else if (ident_start(ch)) {
    bool ascii = true;
    while (p->ls.i < p->len && ident_char((unsigned char)p->src[p->ls.i])) {
      if ((unsigned char)p->src[p->ls.i] >= 0x80) {
        ascii = false;
      }
      advance_byte(p);
    }
    t->kind = T_IDENT;
    t->text = (ozpi_str){p->src + start, (uint32_t)(p->ls.i - start)};
    if (!ascii) {
      if (!p->quiet) {
        ozpi_diag0(p->c, OZPI_D_SYN_nonAsciiIdent, (ozpi_span){s, here(p)}, 0);
      }
    } else {
      for (size_t k = 0; k < sizeof keywords / sizeof keywords[0]; k++) {
        if (ozpi_str_eqz(t->text, keywords[k])) {
          t->kind = (tk)(T_ACTION + k);
          break;
        }
      }
    }
  } else {
    char next = p->ls.i + 1 < p->len ? p->src[p->ls.i + 1] : '\0';
    size_t n = 1;
    switch (ch) {
    case '(':
      t->kind = T_LPAREN;
      push_br(p, '(');
      break;
    case ')':
      t->kind = T_RPAREN;
      if (p->ls.nbr > 0) {
        p->ls.nbr--;
      }
      break;
    case '{':
      t->kind = T_LBRACE;
      push_br(p, '{');
      break;
    case '}':
      t->kind = T_RBRACE;
      if (p->ls.nbr > 0) {
        p->ls.nbr--;
      }
      break;
    case '[':
      t->kind = T_LBRACKET;
      push_br(p, '[');
      break;
    case ']':
      t->kind = T_RBRACKET;
      if (p->ls.nbr > 0) {
        p->ls.nbr--;
      }
      break;
    case ',':
      t->kind = T_COMMA;
      break;
    case ';':
      t->kind = T_SEMI;
      break;
    case ':':
      t->kind = T_COLON;
      break;
    case '+':
      t->kind = T_PLUS;
      break;
    case '-':
      t->kind = T_MINUS;
      break;
    case '*':
      t->kind = T_STAR;
      break;
    case '/':
      t->kind = T_SLASH;
      break;
    case '@':
      t->kind = T_AT;
      break;
    case '$':
      t->kind = T_DOLLAR;
      break;
    case '=':
      t->kind = next == '=' ? T_EQ : T_ASSIGN;
      n = next == '=' ? 2 : 1;
      break;
    case '!':
      t->kind = next == '=' ? T_NE : T_BAD;
      n = next == '=' ? 2 : 1;
      break;
    case '<':
      t->kind = next == '=' ? T_LE : T_LT;
      n = next == '=' ? 2 : 1;
      break;
    case '>':
      t->kind = next == '=' ? T_GE : T_GT;
      n = next == '=' ? 2 : 1;
      break;
    case '.':
      t->kind = next == '.' ? T_DOTDOT : T_BAD;
      n = next == '.' ? 2 : 1;
      break;
    default:
      t->kind = T_BAD;
      break;
    }
    for (size_t k = 0; k < n; k++) {
      advance_byte(p);
    }
    t->text = (ozpi_str){p->src + start, (uint32_t)n};
  }
  t->span = (ozpi_span){s, here(p)};
  if (t->kind == T_NUMBER && t->numerr) {
    lex_diag1(p, OZPI_D_SYN_numberTooLarge, t->span, t->text);
  }
}

static void next(parser *p) {
  p->prev_end = p->tok.span.end;
  lex(p, &p->tok);
}

static token peek(parser *p) {
  lexstate saved = p->ls;
  bool q = p->quiet;
  p->quiet = true;
  token t;
  lex(p, &t);
  p->quiet = q;
  p->ls = saved;
  return t;
}

static ozpi_str tok_text(const token *t) {
  if (t->kind == T_EOF) {
    return ozpi_strz("end of input");
  }
  if (t->kind == T_STRING) {
    return t->text.len > 0 ? t->text : ozpi_strz("string");
  }
  return t->text;
}

static void unexpected(parser *p) {
  if (!p->abort) {
    ozpi_diag1(p->c, OZPI_D_SYN_unexpectedToken, p->tok.span, 0, tok_text(&p->tok));
  }
}

static bool enter(parser *p, ozpi_span at) {
  if (p->abort) {
    return false;
  }
  if (++p->depth > p->c->lim.depth) {
    ozpi_diag1(p->c, OZPI_D_SYN_nestingTooDeep, at, 0, ozpi_fmt_u(p->c, p->c->lim.depth));
    p->abort = true;
    return false;
  }
  return true;
}

static void leave(parser *p) { p->depth--; }

static ozpi_node *mk(parser *p, ozpi_kind kind, ozpi_pos start) {
  ozpi_node *n = ozpi_new(p->c, sizeof(ozpi_node));
  if (n != nullptr) {
    n->kind = kind;
    n->span = (ozpi_span){start, p->prev_end};
  }
  return n;
}

static void finish(parser *p, ozpi_node *n) {
  if (n != nullptr) {
    n->span.end = p->prev_end;
  }
}

static bool add_kid(parser *p, ozpi_node *n, ozpi_node *kid, uint32_t *cap) {
  if (!ozpi_grow(p->c, (void **)&n->kids, cap, sizeof(ozpi_node *), n->nkids + 1)) {
    return false;
  }
  n->kids[n->nkids++] = kid;
  return true;
}

static bool expect(parser *p, tk kind) {
  if (p->tok.kind != kind) {
    unexpected(p);
    return false;
  }
  next(p);
  return true;
}

static bool expect_ident(parser *p, ozpi_str *out, ozpi_span *sp) {
  if (is_kw(p->tok.kind) || p->tok.kind == T_IDENT) {
    if (is_kw(p->tok.kind) && !p->abort) {
      ozpi_diag1(p->c, OZPI_D_SYN_reservedWord, p->tok.span, 0, p->tok.text);
    }
    *out = p->tok.text;
    if (sp != nullptr) {
      *sp = p->tok.span;
    }
    next(p);
    return true;
  }
  unexpected(p);
  return false;
}

static ozpi_node *parse_expr(parser *p);
static ozpi_node *parse_block(parser *p);

static bool expr_start(tk k) {
  return k == T_NUMBER || k == T_STRING || k == T_IDENT || k == T_LPAREN || k == T_LBRACKET || k == T_MINUS ||
         k == T_TRUE || k == T_FALSE || k == T_NOT || k == T_DOLLAR;
}

static bool parse_args(parser *p, ozpi_node *n) {
  uint32_t cap = 0, capn = 0;
  bool named = false;
  if (!enter(p, p->tok.span)) {
    return false;
  }
  next(p);
  if (p->tok.kind != T_RPAREN) {
    for (;;) {
      if (p->tok.kind == T_IDENT && peek(p).kind == T_COLON) {
        ozpi_str name = p->tok.text;
        next(p);
        next(p);
        ozpi_node *v = parse_expr(p);
        if (v == nullptr) {
          leave(p);
          return false;
        }
        if (!ozpi_grow(p->c, (void **)&n->names, &capn, sizeof(ozpi_str), n->nkids - n->npos + 1) ||
            !add_kid(p, n, v, &cap)) {
          leave(p);
          return false;
        }
        n->names[n->nkids - n->npos - 1] = name;
        named = true;
      } else {
        if (named) {
          unexpected(p);
          leave(p);
          return false;
        }
        ozpi_node *v = parse_expr(p);
        if (v == nullptr || !add_kid(p, n, v, &cap)) {
          leave(p);
          return false;
        }
        n->npos++;
      }
      if (p->tok.kind == T_COMMA) {
        next(p);
        continue;
      }
      break;
    }
  }
  leave(p);
  return expect(p, T_RPAREN);
}

static ozpi_node *parse_quantity(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_rat lo = p->tok.num, hi = p->tok.num;
  bool err = p->tok.numerr;
  next(p);
  if (p->tok.kind == T_DOTDOT) {
    next(p);
    if (p->tok.kind != T_NUMBER) {
      unexpected(p);
      return nullptr;
    }
    hi = p->tok.num;
    err = err || p->tok.numerr;
    next(p);
    if (p->tok.kind != T_IDENT) {
      unexpected(p);
      return nullptr;
    }
  }
  if (p->tok.kind != T_IDENT) {
    ozpi_node *n = mk(p, N_NUMBER, start);
    if (n != nullptr) {
      n->lo = lo;
      n->hi = lo;
      n->bad = err;
    }
    return n;
  }
  ozpi_str unit = p->tok.text;
  next(p);
  ozpi_node *n = mk(p, N_QUANTITY, start);
  if (n != nullptr) {
    n->lo = lo;
    n->hi = hi;
    n->text = unit;
    n->bad = err;
  }
  return n;
}

static ozpi_node *parse_primary(parser *p) {
  ozpi_pos start = p->tok.span.start;
  switch (p->tok.kind) {
  case T_NUMBER: {
    ozpi_node *q = parse_quantity(p);
    while (q != nullptr && q->kind == N_QUANTITY && p->tok.kind == T_NUMBER && peek(p).kind == T_IDENT) {
      ozpi_node *r = parse_quantity(p);
      if (r == nullptr) {
        return nullptr;
      }
      ozpi_node *b = mk(p, N_BINARY, start);
      if (b == nullptr) {
        return nullptr;
      }
      uint32_t cap = 0;
      b->op = OP_ADD;
      if (!add_kid(p, b, q, &cap) || !add_kid(p, b, r, &cap)) {
        return nullptr;
      }
      q = b;
    }
    return q;
  }
  case T_STRING: {
    ozpi_node *n = mk(p, N_STRING, start);
    if (n != nullptr) {
      n->text = p->tok.str;
    }
    next(p);
    finish(p, n);
    return n;
  }
  case T_TRUE:
  case T_FALSE: {
    bool v = p->tok.kind == T_TRUE;
    next(p);
    ozpi_node *n = mk(p, N_BOOL, start);
    if (n != nullptr) {
      n->flag = v;
    }
    return n;
  }
  case T_IDENT: {
    ozpi_str name = p->tok.text;
    next(p);
    ozpi_node *n = mk(p, N_NAME, start);
    if (n != nullptr) {
      n->text = name;
    }
    return n;
  }
  case T_DOLLAR: {
    next(p);
    if (p->tok.kind != T_IDENT) {
      unexpected(p);
      return nullptr;
    }
    ozpi_str name = p->tok.text;
    next(p);
    ozpi_node *n = mk(p, N_SPLICE, start);
    if (n != nullptr) {
      n->text = name;
      if (!p->in_quote) {
        ozpi_diag0(p->c, OZPI_D_SYN_spliceOutsideQuote, n->span, 0);
        n->bad = true;
      }
    }
    return n;
  }
  case T_LBRACKET: {
    if (!enter(p, p->tok.span)) {
      return nullptr;
    }
    next(p);
    ozpi_node *n = mk(p, N_LIST, start);
    uint32_t cap = 0;
    if (n == nullptr) {
      leave(p);
      return nullptr;
    }
    if (p->tok.kind != T_RBRACKET) {
      for (;;) {
        ozpi_node *e = parse_expr(p);
        if (e == nullptr || !add_kid(p, n, e, &cap)) {
          leave(p);
          return nullptr;
        }
        if (p->tok.kind == T_COMMA) {
          next(p);
          continue;
        }
        break;
      }
    }
    leave(p);
    if (!expect(p, T_RBRACKET)) {
      return nullptr;
    }
    finish(p, n);
    return n;
  }
  case T_LPAREN: {
    if (!enter(p, p->tok.span)) {
      return nullptr;
    }
    next(p);
    ozpi_node *e = parse_expr(p);
    leave(p);
    if (e == nullptr || !expect(p, T_RPAREN)) {
      return nullptr;
    }
    return e;
  }
  case T_QUOTE: {
    ozpi_span at = p->tok.span;
    if (!p->abort) {
      ozpi_diag0(p->c, OZPI_D_SYN_quotePlacement, at, 0);
    }
    next(p);
    bool saved = p->in_quote;
    p->in_quote = true;
    ozpi_node *blk = p->tok.kind == T_LBRACE ? parse_block(p) : nullptr;
    p->in_quote = saved;
    if (blk == nullptr) {
      return nullptr;
    }
    ozpi_node *n = mk(p, N_ERROR, start);
    if (n != nullptr) {
      n->bad = true;
    }
    return n;
  }
  default:
    unexpected(p);
    return nullptr;
  }
}

static ozpi_node *parse_postfix(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *n = parse_primary(p);
  if (n == nullptr || p->tok.kind != T_LPAREN) {
    return n;
  }
  if (n->kind != N_NAME) {
    unexpected(p);
    return nullptr;
  }
  ozpi_node *call = mk(p, N_CALL, start);
  if (call == nullptr) {
    return nullptr;
  }
  call->text = n->text;
  call->span2 = n->span;
  if (!parse_args(p, call)) {
    return nullptr;
  }
  finish(p, call);
  return call;
}

static ozpi_node *unary_node(parser *p, ozpi_op op, ozpi_pos start, ozpi_node *(*sub)(parser *)) {
  if (!enter(p, p->tok.span)) {
    return nullptr;
  }
  next(p);
  ozpi_node *e = sub(p);
  leave(p);
  if (e == nullptr) {
    return nullptr;
  }
  ozpi_node *n = mk(p, N_UNARY, start);
  uint32_t cap = 0;
  if (n == nullptr || !add_kid(p, n, e, &cap)) {
    return nullptr;
  }
  n->op = op;
  return n;
}

static ozpi_node *parse_unary(parser *p) {
  if (p->tok.kind == T_MINUS) {
    return unary_node(p, OP_NEG, p->tok.span.start, parse_unary);
  }
  return parse_postfix(p);
}

static ozpi_node *binary(parser *p, ozpi_pos start, ozpi_op op, ozpi_node *l, ozpi_node *r) {
  if (l == nullptr || r == nullptr) {
    return nullptr;
  }
  ozpi_node *b = mk(p, N_BINARY, start);
  uint32_t cap = 0;
  if (b == nullptr || !add_kid(p, b, l, &cap) || !add_kid(p, b, r, &cap)) {
    return nullptr;
  }
  b->op = op;
  return b;
}

static ozpi_node *parse_prod(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *l = parse_unary(p);
  while (l != nullptr && (p->tok.kind == T_STAR || p->tok.kind == T_SLASH)) {
    ozpi_op op = p->tok.kind == T_STAR ? OP_MUL : OP_DIV;
    next(p);
    l = binary(p, start, op, l, parse_unary(p));
  }
  return l;
}

static ozpi_node *parse_sum(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *l = parse_prod(p);
  while (l != nullptr && (p->tok.kind == T_PLUS || p->tok.kind == T_MINUS)) {
    ozpi_op op = p->tok.kind == T_PLUS ? OP_ADD : OP_SUB;
    next(p);
    l = binary(p, start, op, l, parse_prod(p));
  }
  return l;
}

static ozpi_node *parse_cmp(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *l = parse_sum(p);
  ozpi_op op = OP_NONE;
  switch (p->tok.kind) {
  case T_EQ:
    op = OP_EQ;
    break;
  case T_NE:
    op = OP_NE;
    break;
  case T_LT:
    op = OP_LT;
    break;
  case T_LE:
    op = OP_LE;
    break;
  case T_GT:
    op = OP_GT;
    break;
  case T_GE:
    op = OP_GE;
    break;
  default:
    return l;
  }
  if (l == nullptr) {
    return nullptr;
  }
  next(p);
  return binary(p, start, op, l, parse_sum(p));
}

static ozpi_node *parse_not(parser *p) {
  if (p->tok.kind == T_NOT) {
    return unary_node(p, OP_NOT, p->tok.span.start, parse_not);
  }
  return parse_cmp(p);
}

static ozpi_node *parse_and(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *l = parse_not(p);
  while (l != nullptr && p->tok.kind == T_AND) {
    next(p);
    l = binary(p, start, OP_AND, l, parse_not(p));
  }
  return l;
}

static ozpi_node *parse_expr(parser *p) {
  if (p->abort || p->c->oom) {
    return nullptr;
  }
  ozpi_pos start = p->tok.span.start;
  ozpi_node *l = parse_and(p);
  while (l != nullptr && p->tok.kind == T_OR) {
    next(p);
    l = binary(p, start, OP_OR, l, parse_and(p));
  }
  return l;
}

static bool at_stmt_end(const parser *p) {
  tk k = p->tok.kind;
  return k == T_NEWLINE || k == T_SEMI || k == T_RBRACE || k == T_EOF;
}

static void sync_stmt(parser *p, uint32_t level) {
  while (!p->abort) {
    tk k = p->tok.kind;
    if (k == T_EOF) {
      return;
    }
    if (k == T_RBRACE && p->ls.nbr + 1 == level) {
      return;
    }
    if ((k == T_NEWLINE || k == T_SEMI) && p->ls.nbr == level) {
      next(p);
      return;
    }
    next(p);
  }
}

static ozpi_node *parse_if(parser *p) {
  ozpi_pos start = p->tok.span.start;
  next(p);
  ozpi_node *cond = parse_expr(p);
  if (cond == nullptr) {
    return nullptr;
  }
  ozpi_node *then = parse_block(p);
  if (then == nullptr) {
    return nullptr;
  }
  ozpi_node *n = mk(p, N_IF, start);
  uint32_t cap = 0;
  if (n == nullptr || !add_kid(p, n, cond, &cap) || !add_kid(p, n, then, &cap)) {
    return nullptr;
  }
  if (p->tok.kind == T_ELSE) {
    next(p);
    ozpi_node *e;
    if (p->tok.kind == T_IF) {
      if (!enter(p, p->tok.span)) {
        return nullptr;
      }
      e = parse_if(p);
      leave(p);
    } else {
      e = parse_block(p);
    }
    if (e == nullptr || !add_kid(p, n, e, &cap)) {
      return nullptr;
    }
  }
  finish(p, n);
  return n;
}

static ozpi_node *parse_ingredients(parser *p) {
  ozpi_pos start = p->tok.span.start;
  next(p);
  if (p->tok.kind != T_LBRACE) {
    unexpected(p);
    return nullptr;
  }
  if (!enter(p, p->tok.span)) {
    return nullptr;
  }
  next(p);
  uint32_t level = p->ls.nbr;
  ozpi_node *n = mk(p, N_INGREDIENTS, start);
  uint32_t cap = 0;
  if (n == nullptr) {
    leave(p);
    return nullptr;
  }
  for (;;) {
    while (p->tok.kind == T_NEWLINE || p->tok.kind == T_SEMI) {
      next(p);
    }
    if (p->tok.kind == T_RBRACE || p->tok.kind == T_EOF || p->abort) {
      break;
    }
    ozpi_pos is = p->tok.span.start;
    ozpi_str name;
    ozpi_node *amount = nullptr;
    if (expect_ident(p, &name, nullptr) && expect(p, T_COLON)) {
      if (p->tok.kind == T_IDENT && expr_start(peek(p).kind)) {
        next(p);
      }
      amount = parse_expr(p);
    }
    ozpi_node *item = amount == nullptr ? nullptr : mk(p, N_ITEM, is);
    if (item != nullptr) {
      uint32_t c2 = 0;
      item->text = name;
      if (!add_kid(p, item, amount, &c2) || !add_kid(p, n, item, &cap)) {
        leave(p);
        return nullptr;
      }
      if (at_stmt_end(p)) {
        continue;
      }
      unexpected(p);
    }
    sync_stmt(p, level);
  }
  leave(p);
  if (!expect(p, T_RBRACE)) {
    return nullptr;
  }
  finish(p, n);
  return n;
}

static ozpi_node *parse_stmt(parser *p) {
  ozpi_pos start = p->tok.span.start;
  ozpi_node *n = nullptr;
  switch (p->tok.kind) {
  case T_INGREDIENTS:
    n = parse_ingredients(p);
    break;
  case T_LET: {
    next(p);
    bool splice = false;
    ozpi_str name = {0};
    if (p->tok.kind == T_DOLLAR) {
      ozpi_pos ds = p->tok.span.start;
      next(p);
      if (p->tok.kind != T_IDENT) {
        unexpected(p);
        return nullptr;
      }
      name = p->tok.text;
      next(p);
      splice = true;
      if (!p->in_quote) {
        ozpi_diag0(p->c, OZPI_D_SYN_spliceOutsideQuote, (ozpi_span){ds, p->prev_end}, 0);
      }
    } else if (!expect_ident(p, &name, nullptr)) {
      return nullptr;
    }
    if (!expect(p, T_ASSIGN)) {
      return nullptr;
    }
    ozpi_node *v = parse_expr(p);
    if (v == nullptr) {
      return nullptr;
    }
    n = mk(p, N_LET, start);
    uint32_t cap = 0;
    if (n == nullptr || !add_kid(p, n, v, &cap)) {
      return nullptr;
    }
    n->text = name;
    n->flag = splice;
    break;
  }
  case T_IF:
    n = parse_if(p);
    break;
  case T_FOR: {
    next(p);
    ozpi_str var;
    if (!expect_ident(p, &var, nullptr) || !expect(p, T_IN)) {
      return nullptr;
    }
    ozpi_node *list = parse_expr(p);
    ozpi_node *body = list == nullptr ? nullptr : parse_block(p);
    if (body == nullptr) {
      return nullptr;
    }
    n = mk(p, N_FOR, start);
    uint32_t cap = 0;
    if (n == nullptr || !add_kid(p, n, list, &cap) || !add_kid(p, n, body, &cap)) {
      return nullptr;
    }
    n->text = var;
    break;
  }
  case T_REPEAT: {
    next(p);
    ozpi_node *count = parse_expr(p);
    ozpi_node *body = count == nullptr ? nullptr : parse_block(p);
    if (body == nullptr) {
      return nullptr;
    }
    n = mk(p, N_REPEAT, start);
    uint32_t cap = 0;
    if (n == nullptr || !add_kid(p, n, count, &cap) || !add_kid(p, n, body, &cap)) {
      return nullptr;
    }
    break;
  }
  default:
    n = parse_expr(p);
    break;
  }
  if (n == nullptr) {
    return nullptr;
  }
  uint32_t capa = 0;
  while (p->tok.kind == T_AT) {
    ozpi_pos as = p->tok.span.start;
    next(p);
    if (p->tok.kind != T_IDENT) {
      unexpected(p);
      return nullptr;
    }
    ozpi_str name = p->tok.text;
    next(p);
    ozpi_node *a = mk(p, N_ANNOT, as);
    if (a == nullptr) {
      return nullptr;
    }
    a->text = name;
    if (p->tok.kind == T_LPAREN && !parse_args(p, a)) {
      return nullptr;
    }
    finish(p, a);
    if (!ozpi_grow(p->c, (void **)&n->annots, &capa, sizeof(ozpi_node *), n->nannots + 1)) {
      return nullptr;
    }
    n->annots[n->nannots++] = a;
  }
  if (!at_stmt_end(p)) {
    unexpected(p);
    return nullptr;
  }
  return n;
}

static ozpi_node *parse_block(parser *p) {
  ozpi_pos start = p->tok.span.start;
  if (p->tok.kind != T_LBRACE) {
    unexpected(p);
    return nullptr;
  }
  if (!enter(p, p->tok.span)) {
    return nullptr;
  }
  next(p);
  uint32_t level = p->ls.nbr;
  ozpi_node *b = mk(p, N_BLOCK, start);
  uint32_t cap = 0;
  if (b == nullptr) {
    leave(p);
    return nullptr;
  }
  bool failed = false;
  for (;;) {
    while (p->tok.kind == T_NEWLINE || p->tok.kind == T_SEMI) {
      next(p);
    }
    if (p->abort || p->c->oom) {
      leave(p);
      return nullptr;
    }
    if (p->tok.kind == T_RBRACE) {
      break;
    }
    if (p->tok.kind == T_EOF) {
      unexpected(p);
      leave(p);
      return nullptr;
    }
    ozpi_node *s = parse_stmt(p);
    if (s == nullptr) {
      failed = true;
      sync_stmt(p, level);
      continue;
    }
    if (!add_kid(p, b, s, &cap)) {
      leave(p);
      return nullptr;
    }
  }
  leave(p);
  next(p);
  finish(p, b);
  if (failed) {
    b->bad = true;
  }
  return b;
}

static void sync_top(parser *p) {
  while (p->tok.kind != T_EOF && !(p->tok.kind == T_NEWLINE && p->ls.nbr == 0)) {
    next(p);
  }
}

static bool word_at(const parser *p, size_t i, const char *w) {
  size_t n = ozpi_strlen(w);
  if (i + n > p->len || memcmp(p->src + i, w, n) != 0) {
    return false;
  }
  return i + n == p->len || !ident_char((unsigned char)p->src[i + n]);
}

static void resync_after_abort(parser *p) {
  size_t i = p->ls.i;
  uint32_t line = p->ls.line;
  for (;;) {
    while (i < p->len && p->src[i] != '\n') {
      i++;
    }
    if (i >= p->len) {
      break;
    }
    i++;
    line++;
    if (word_at(p, i, "action") || word_at(p, i, "macro") || word_at(p, i, "recipe")) {
      break;
    }
  }
  p->ls = (lexstate){i, line, 1, 0};
  p->abort = false;
  p->depth = 0;
  p->in_quote = false;
  lex(p, &p->tok);
}

static bool parse_param_names(parser *p, ozpi_decl *d, bool defaults) {
  uint32_t cap = 0, capd = 0;
  if (!expect(p, T_LPAREN)) {
    return false;
  }
  if (p->tok.kind != T_RPAREN) {
    for (;;) {
      ozpi_str name;
      if (!expect_ident(p, &name, nullptr)) {
        return false;
      }
      ozpi_node *def = nullptr;
      if (defaults) {
        if (!expect(p, T_COLON)) {
          return false;
        }
        def = parse_expr(p);
        if (def == nullptr) {
          return false;
        }
      }
      if (!ozpi_grow(p->c, (void **)&d->params, &cap, sizeof(ozpi_str), d->nparams + 1) ||
          !ozpi_grow(p->c, (void **)&d->defaults, &capd, sizeof(ozpi_node *), d->nparams + 1)) {
        return false;
      }
      d->params[d->nparams] = name;
      d->defaults[d->nparams] = def;
      d->nparams++;
      if (p->tok.kind == T_COMMA) {
        next(p);
        continue;
      }
      break;
    }
  }
  return expect(p, T_RPAREN);
}

static void skip_braces(parser *p) {
  if (p->tok.kind != T_LBRACE) {
    return;
  }
  uint32_t level = p->ls.nbr;
  next(p);
  while (p->tok.kind != T_EOF && !(p->tok.kind == T_RBRACE && p->ls.nbr + 1 == level)) {
    next(p);
  }
  if (p->tok.kind == T_RBRACE) {
    next(p);
  }
}

static bool parse_macro_body(parser *p, ozpi_decl *d) {
  if (p->tok.kind != T_LBRACE) {
    unexpected(p);
    return false;
  }
  lexstate saved = p->ls;
  token saved_tok = p->tok;
  ozpi_pos saved_prev = p->prev_end;
  bool q = p->quiet;
  p->quiet = true;
  next(p);
  while (p->tok.kind == T_NEWLINE) {
    next(p);
  }
  bool shape = p->tok.kind == T_QUOTE;
  if (shape) {
    next(p);
    shape = p->tok.kind == T_LBRACE;
    if (shape) {
      skip_braces(p);
      while (p->tok.kind == T_NEWLINE) {
        next(p);
      }
      shape = p->tok.kind == T_RBRACE;
    }
  }
  p->quiet = q;
  p->ls = saved;
  p->tok = saved_tok;
  p->prev_end = saved_prev;
  if (!shape) {
    ozpi_diag1(p->c, OZPI_D_SYN_macroBody, d->span, 0, d->name);
    skip_braces(p);
    return true;
  }
  if (!enter(p, p->tok.span)) {
    return false;
  }
  next(p);
  while (p->tok.kind == T_NEWLINE) {
    next(p);
  }
  next(p);
  p->in_quote = true;
  d->body = parse_block(p);
  p->in_quote = false;
  while (p->tok.kind == T_NEWLINE) {
    next(p);
  }
  leave(p);
  if (d->body == nullptr) {
    return false;
  }
  return expect(p, T_RBRACE);
}

static void parse_decl(parser *p) {
  ozpi_ctx *c = p->c;
  if (!ozpi_grow(c, (void **)&c->decls, &c->capdecls, sizeof(ozpi_decl), c->ndecls + 1)) {
    return;
  }
  ozpi_decl d = {0};
  tk kind = p->tok.kind;
  d.kind = kind == T_ACTION ? D_ACTION : kind == T_MACRO ? D_MACRO : D_RECIPE;
  next(p);
  bool ok = expect_ident(p, &d.name, &d.span);
  if (ok) {
    ok = parse_param_names(p, &d, d.kind == D_RECIPE);
  }
  if (ok && d.kind == D_MACRO) {
    ok = parse_macro_body(p, &d);
  } else if (ok && d.kind == D_RECIPE) {
    d.body = parse_block(p);
    ok = d.body != nullptr;
  }
  if (ok && !(p->tok.kind == T_NEWLINE || p->tok.kind == T_SEMI || p->tok.kind == T_EOF)) {
    unexpected(p);
    ok = false;
  }
  if (ok || d.kind == D_ACTION || d.body != nullptr) {
    c->decls[c->ndecls++] = d;
  }
  if (p->abort) {
    resync_after_abort(p);
  } else if (!ok) {
    sync_top(p);
  }
}

static void parser_init(parser *p, ozpi_ctx *c, const char *src, size_t len, uint32_t line, uint32_t col, size_t at) {
  *p = (parser){0};
  p->c = c;
  p->src = src;
  p->len = len;
  p->ls = (lexstate){at, line, col, 0};
  lex(p, &p->tok);
}

void ozpi_parse_file(ozpi_ctx *c, const char *src, size_t len) {
  parser p;
  parser_init(&p, c, src, len, 1, 1, 0);
  while (!c->oom) {
    while (p.tok.kind == T_NEWLINE || p.tok.kind == T_SEMI) {
      next(&p);
    }
    if (p.tok.kind == T_EOF) {
      break;
    }
    if (p.tok.kind == T_ACTION || p.tok.kind == T_MACRO || p.tok.kind == T_RECIPE) {
      parse_decl(&p);
      continue;
    }
    unexpected(&p);
    sync_top(&p);
  }
}

ozpi_node *ozpi_parse_expr_text(ozpi_ctx *c, const char *src, size_t len, ozpi_span *where) {
  parser p;
  parser_init(&p, c, src, len, where->start.line, where->start.col, where->start.col - 1);
  ozpi_node *n = parse_expr(&p);
  if (n != nullptr && p.tok.kind != T_EOF && p.tok.kind != T_SEMI) {
    unexpected(&p);
    return nullptr;
  }
  return n;
}

bool ozpi_check_utf8(const char *src, size_t len, ozpi_pos *bad) {
  uint32_t line = 1, col = 1;
  size_t i = 0;
  while (i < len) {
    unsigned char b = (unsigned char)src[i];
    size_t n = 0;
    uint32_t cp = 0, min = 0;
    if (b < 0x80) {
      n = 1;
    } else if (b >= 0xc2 && b <= 0xdf) {
      n = 2;
      cp = b & 0x1fu;
      min = 0x80;
    } else if (b >= 0xe0 && b <= 0xef) {
      n = 3;
      cp = b & 0x0fu;
      min = 0x800;
    } else if (b >= 0xf0 && b <= 0xf4) {
      n = 4;
      cp = b & 0x07u;
      min = 0x10000;
    }
    bool ok = n > 0 && i + n <= len;
    for (size_t k = 1; ok && k < n; k++) {
      unsigned char cb = (unsigned char)src[i + k];
      ok = (cb & 0xc0u) == 0x80u;
      cp = (cp << 6) | (cb & 0x3fu);
    }
    if (ok && n > 1 && (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))) {
      ok = false;
    }
    if (!ok) {
      *bad = (ozpi_pos){line, col};
      return false;
    }
    if (b == '\n') {
      line++;
      col = 1;
    } else {
      col += (uint32_t)n;
    }
    i += n;
  }
  return true;
}
