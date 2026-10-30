/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_LEX_H
#define OZNI_LEX_H

#include "ctx.h"

typedef enum ozni_tok_kind : uint8_t {
  OZNI_T_EOF,
  OZNI_T_SEP,
  OZNI_T_IDENT,
  OZNI_T_KEYWORD,
  OZNI_T_NUMBER,
  OZNI_T_FRACTION,
  OZNI_T_STRING,
  OZNI_T_MLSTRING,
  OZNI_T_LPAREN,
  OZNI_T_RPAREN,
  OZNI_T_LBRACE,
  OZNI_T_RBRACE,
  OZNI_T_LBRACKET,
  OZNI_T_RBRACKET,
  OZNI_T_LT,
  OZNI_T_GT,
  OZNI_T_LE,
  OZNI_T_GE,
  OZNI_T_EQEQ,
  OZNI_T_NE,
  OZNI_T_EQ,
  OZNI_T_COLON,
  OZNI_T_COMMA,
  OZNI_T_DOT,
  OZNI_T_DOTDOT,
  OZNI_T_PLUS,
  OZNI_T_MINUS,
  OZNI_T_STAR,
  OZNI_T_SLASH,
  OZNI_T_QUESTION,
  OZNI_T_PIPE,
  OZNI_T_AT,
  OZNI_T_DOLLAR,
  OZNI_T_PERCENT,
  OZNI_T_ARROW,
  OZNI_T_LINK,
  OZNI_T_PIPE_GT,
} ozni_tok_kind;

typedef struct ozni_token {
  ozni_tok_kind kind;
  uint32_t start;
  uint32_t end;
  ozni_index text;
} ozni_token;

[[nodiscard]] bool ozni_lex(ozni_ctx *c, ozni_index file, ozni_table *tokens);
bool ozni_is_reserved(const char *s, size_t n);

#endif
