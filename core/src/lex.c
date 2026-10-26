/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "lex.h"

static const char *const reserved[] = {
    "and",    "assemble", "branch",  "buy",   "check",   "choose", "else",  "false",   "finer",
    "for",    "if",       "import",  "in",    "ingredients", "let", "local", "macro", "make",
    "module", "not",      "or",      "package", "parallel", "phrases", "plate", "quote", "repeat",
    "requires", "serve",  "steps",   "takes", "true",    "uses",   "variant", "where",
};

bool ozni_is_reserved(const char *s, size_t n) {
  for (size_t i = 0; i < sizeof reserved / sizeof reserved[0]; i++) {
    if (strlen(reserved[i]) == n && memcmp(reserved[i], s, n) == 0) {
      return true;
    }
  }
  return false;
}

typedef struct lexer {
  ozni_ctx *c;
  ozni_index file;
  const char *s;
  uint32_t n;
  uint32_t i;
  int depth;
  ozni_table *out;
  ozni_table *buf;
  bool ok;
} lexer;

static bool is_alpha(unsigned char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; }
static bool is_digit(unsigned char ch) { return ch >= '0' && ch <= '9'; }

static void diag(lexer *lx, ozni_diag_id id, uint32_t start, uint32_t end, const char *arg, size_t arglen) {
  ozni_str a = {arg, (uint32_t)arglen};
  if (!ozni_diag_add(lx->c, id, (ozni_span){lx->file, start, end}, 0, &a, arg != nullptr ? 1 : 0)) {
    lx->ok = false;
  }
}

static void push(lexer *lx, ozni_tok_kind kind, uint32_t start, uint32_t end, const char *text, size_t len) {
  ozni_token t = {kind, start, end, 0};
  if (text != nullptr && !ozni_intern_put(lx->c->names, text, len, &t.text)) {
    lx->ok = false;
    return;
  }
  if (!ozni_table_push(lx->out, &t, nullptr)) {
    lx->ok = false;
  }
}

static bool fits_int64(const char *d, size_t n) {
  while (n > 1 && *d == '0') {
    d++;
    n--;
  }
  static const char max[] = "9223372036854775807";
  if (n != sizeof max - 1) {
    return n < sizeof max - 1;
  }
  return memcmp(d, max, n) <= 0;
}

static void lex_number(lexer *lx) {
  uint32_t start = lx->i;
  while (lx->i < lx->n && is_digit((unsigned char)lx->s[lx->i])) {
    lx->i++;
  }
  uint32_t int_end = lx->i;
  bool too_large = !fits_int64(lx->s + start, int_end - start);
  ozni_tok_kind kind = OZNI_T_NUMBER;
  if (lx->i + 1 < lx->n && lx->s[lx->i] == '.' && is_digit((unsigned char)lx->s[lx->i + 1])) {
    lx->i++;
    uint32_t frac = lx->i;
    while (lx->i < lx->n && is_digit((unsigned char)lx->s[lx->i])) {
      lx->i++;
    }
    char digits[64];
    size_t total = (int_end - start) + (lx->i - frac);
    if (lx->i - frac > 18 || total >= sizeof digits) {
      too_large = true;
    } else {
      memcpy(digits, lx->s + start, int_end - start);
      memcpy(digits + (int_end - start), lx->s + frac, lx->i - frac);
      too_large = too_large || !fits_int64(digits, total);
    }
  } else if (lx->i + 1 < lx->n && lx->s[lx->i] == '/' && is_digit((unsigned char)lx->s[lx->i + 1])) {
    lx->i++;
    uint32_t den = lx->i;
    while (lx->i < lx->n && is_digit((unsigned char)lx->s[lx->i])) {
      lx->i++;
    }
    too_large = too_large || !fits_int64(lx->s + den, lx->i - den);
    kind = OZNI_T_FRACTION;
  }
  if (too_large) {
    diag(lx, OZNI_D_SYN_numberTooLarge, start, lx->i, nullptr, 0);
  }
  push(lx, kind, start, lx->i, lx->s + start, lx->i - start);
}

static void lex_word(lexer *lx) {
  uint32_t start = lx->i;
  bool ascii = true;
  while (lx->i < lx->n) {
    unsigned char ch = (unsigned char)lx->s[lx->i];
    if (ch >= 0x80) {
      ascii = false;
    } else if (!is_alpha(ch) && !is_digit(ch)) {
      break;
    }
    lx->i++;
  }
  const char *w = lx->s + start;
  size_t len = lx->i - start;
  if (!ascii) {
    diag(lx, OZNI_D_SYN_nonAsciiIdent, start, lx->i, w, len);
  }
  push(lx, ascii && ozni_is_reserved(w, len) ? OZNI_T_KEYWORD : OZNI_T_IDENT, start, lx->i, w, len);
}

static bool buf_put(lexer *lx, char ch) {
  if (!ozni_table_push(lx->buf, &ch, nullptr)) {
    lx->ok = false;
    return false;
  }
  return true;
}

static void push_buffer(lexer *lx, ozni_tok_kind kind, uint32_t start) {
  ozni_index n = ozni_table_count(lx->buf);
  char small[256];
  char *text = small;
  if (n > sizeof small) {
    text = ozni_arena_alloc(lx->c->arena, n);
    if (text == nullptr) {
      lx->ok = false;
      return;
    }
  }
  for (ozni_index k = 0; k < n; k++) {
    text[k] = *(const char *)ozni_table_get(lx->buf, k);
  }
  push(lx, kind, start, lx->i, text, n);
}

static void lex_string(lexer *lx) {
  uint32_t start = lx->i;
  lx->buf = ozni_table_new(lx->c->arena, 1);
  if (lx->buf == nullptr) {
    lx->ok = false;
    return;
  }
  if (lx->i + 2 < lx->n && lx->s[lx->i + 1] == '"' && lx->s[lx->i + 2] == '"') {
    lx->i += 3;
    while (lx->i < lx->n && !(lx->i + 2 < lx->n && lx->s[lx->i] == '"' && lx->s[lx->i + 1] == '"' &&
                              lx->s[lx->i + 2] == '"')) {
      if (!buf_put(lx, lx->s[lx->i])) {
        return;
      }
      lx->i++;
    }
    if (lx->i >= lx->n) {
      diag(lx, OZNI_D_SYN_unterminatedString, start, lx->i, nullptr, 0);
    } else {
      lx->i += 3;
    }
    push_buffer(lx, OZNI_T_MLSTRING, start);
    return;
  }
  lx->i++;
  while (lx->i < lx->n && lx->s[lx->i] != '"' && lx->s[lx->i] != '\n') {
    char ch = lx->s[lx->i];
    if (ch == '\\' && lx->i + 1 < lx->n && lx->s[lx->i + 1] != '\n') {
      char e = lx->s[lx->i + 1];
      if (e == '"' || e == '\\') {
        ch = e;
      } else if (e == 'n') {
        ch = '\n';
      } else {
        diag(lx, OZNI_D_SYN_badEscape, lx->i, lx->i + 2, lx->s + lx->i, 2);
        ch = e;
      }
      lx->i++;
    }
    if (!buf_put(lx, ch)) {
      return;
    }
    lx->i++;
  }
  if (lx->i >= lx->n || lx->s[lx->i] == '\n') {
    diag(lx, OZNI_D_SYN_unterminatedString, start, lx->i, nullptr, 0);
  } else {
    lx->i++;
  }
  push_buffer(lx, OZNI_T_STRING, start);
}

static bool skip_space_and_comments(lexer *lx) {
  while (lx->i < lx->n) {
    char ch = lx->s[lx->i];
    if (ch == ' ' || ch == '\t' || ch == '\r') {
      lx->i++;
    } else if (ch == '\n' && lx->depth > 0) {
      lx->i++;
    } else if (ch == '/' && lx->i + 1 < lx->n && lx->s[lx->i + 1] == '/') {
      while (lx->i < lx->n && lx->s[lx->i] != '\n') {
        lx->i++;
      }
    } else if (ch == '/' && lx->i + 1 < lx->n && lx->s[lx->i + 1] == '*') {
      uint32_t start = lx->i;
      lx->i += 2;
      while (lx->i < lx->n && !(lx->s[lx->i] == '*' && lx->i + 1 < lx->n && lx->s[lx->i + 1] == '/')) {
        lx->i++;
      }
      if (lx->i >= lx->n) {
        diag(lx, OZNI_D_SYN_unterminatedComment, start, lx->n, nullptr, 0);
        return false;
      }
      lx->i += 2;
    } else {
      break;
    }
  }
  return true;
}

typedef struct punct {
  const char *text;
  ozni_tok_kind kind;
} punct;

static const punct puncts[] = {
    {"<->", OZNI_T_LINK}, {"->", OZNI_T_ARROW}, {"<=", OZNI_T_LE},   {">=", OZNI_T_GE},     {"==", OZNI_T_EQEQ},
    {"!=", OZNI_T_NE},    {"..", OZNI_T_DOTDOT}, {"(", OZNI_T_LPAREN}, {")", OZNI_T_RPAREN}, {"{", OZNI_T_LBRACE},
    {"}", OZNI_T_RBRACE}, {"[", OZNI_T_LBRACKET}, {"]", OZNI_T_RBRACKET}, {"<", OZNI_T_LT},   {">", OZNI_T_GT},
    {"=", OZNI_T_EQ},     {":", OZNI_T_COLON},  {",", OZNI_T_COMMA},  {".", OZNI_T_DOT},    {"+", OZNI_T_PLUS},
    {"-", OZNI_T_MINUS},  {"*", OZNI_T_STAR},   {"/", OZNI_T_SLASH},  {"?", OZNI_T_QUESTION}, {"|", OZNI_T_PIPE},
    {"@", OZNI_T_AT},     {"$", OZNI_T_DOLLAR}, {"%", OZNI_T_PERCENT}, {";", OZNI_T_SEP},
};

bool ozni_lex(ozni_ctx *c, ozni_index file, ozni_table *tokens) {
  const ozni_source *src = &c->srcs[file];
  lexer lx = {c, file, src->text, (uint32_t)src->len, 0, 0, tokens, nullptr, true};
  while (lx.ok) {
    if (!skip_space_and_comments(&lx) || lx.i >= lx.n) {
      break;
    }
    unsigned char ch = (unsigned char)lx.s[lx.i];
    if (ch == '\n') {
      push(&lx, OZNI_T_SEP, lx.i, lx.i + 1, nullptr, 0);
      lx.i++;
    } else if (is_digit(ch)) {
      lex_number(&lx);
    } else if (is_alpha(ch) || ch >= 0x80) {
      lex_word(&lx);
    } else if (ch == '"') {
      lex_string(&lx);
    } else {
      const punct *p = nullptr;
      for (size_t k = 0; k < sizeof puncts / sizeof puncts[0] && p == nullptr; k++) {
        size_t len = strlen(puncts[k].text);
        if (lx.i + len <= lx.n && memcmp(lx.s + lx.i, puncts[k].text, len) == 0) {
          p = &puncts[k];
        }
      }
      if (p == nullptr) {
        diag(&lx, OZNI_D_SYN_unexpectedCharacter, lx.i, lx.i + 1, lx.s + lx.i, 1);
        lx.i++;
        continue;
      }
      uint32_t len = (uint32_t)strlen(p->text);
      if (p->kind == OZNI_T_LPAREN || p->kind == OZNI_T_LBRACKET) {
        lx.depth++;
      } else if ((p->kind == OZNI_T_RPAREN || p->kind == OZNI_T_RBRACKET) && lx.depth > 0) {
        lx.depth--;
      }
      push(&lx, p->kind, lx.i, lx.i + len, p->kind == OZNI_T_SEP ? ";" : nullptr, p->kind == OZNI_T_SEP ? 1 : 0);
      lx.i += len;
    }
  }
  if (lx.ok) {
    push(&lx, OZNI_T_EOF, lx.n, lx.n, nullptr, 0);
  }
  return lx.ok;
}
