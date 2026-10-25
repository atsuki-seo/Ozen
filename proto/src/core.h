/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZPI_CORE_H
#define OZPI_CORE_H

#include "num.h"
#include "ozpi.h"

typedef struct ozpi_pos {
  uint32_t line, col;
} ozpi_pos;

typedef struct ozpi_span {
  ozpi_pos start, end;
} ozpi_span;

typedef struct ozpi_str {
  const char *p;
  uint32_t len;
} ozpi_str;

typedef enum ozpi_dim : uint8_t { DIM_NONE, DIM_MASS, DIM_VOLUME, DIM_LENGTH, DIM_TIME, DIM_COUNT } ozpi_dim;

typedef enum ozpi_kind : uint8_t {
  N_BLOCK,
  N_INGREDIENTS,
  N_ITEM,
  N_LET,
  N_IF,
  N_FOR,
  N_REPEAT,
  N_CALL,
  N_NAME,
  N_SPLICE,
  N_NUMBER,
  N_QUANTITY,
  N_STRING,
  N_BOOL,
  N_LIST,
  N_BINARY,
  N_UNARY,
  N_ANNOT,
  N_QUOTE,
  N_ERROR
} ozpi_kind;

typedef enum ozpi_op : uint8_t {
  OP_NONE,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_EQ,
  OP_NE,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_AND,
  OP_OR,
  OP_NEG,
  OP_NOT
} ozpi_op;

typedef struct ozpi_node ozpi_node;
struct ozpi_node {
  ozpi_kind kind;
  ozpi_op op;
  bool flag;
  bool bad;
  uint32_t exp;
  uint32_t bexp;
  ozpi_span span;
  ozpi_span span2;
  ozpi_str text;
  ozpi_rat lo, hi;
  ozpi_node **kids;
  uint32_t nkids;
  ozpi_str *names;
  uint32_t npos;
  ozpi_node **annots;
  uint32_t nannots;
};

typedef enum ozpi_decl_kind : uint8_t { D_ACTION, D_MACRO, D_RECIPE } ozpi_decl_kind;

typedef struct ozpi_decl {
  ozpi_decl_kind kind;
  ozpi_str name;
  ozpi_span span;
  ozpi_str *params;
  ozpi_node **defaults;
  uint32_t nparams;
  ozpi_node *body;
} ozpi_decl;

typedef struct ozpi_expansion {
  ozpi_span call;
  uint32_t parent;
} ozpi_expansion;

typedef struct ozpi_diag {
  ozpi_diag_id id;
  ozpi_span span;
  uint32_t exp;
  ozpi_str *args;
  uint32_t nargs;
} ozpi_diag;

typedef struct ozpi_buf {
  char *p;
  size_t len, cap;
} ozpi_buf;

typedef struct ozpi_step {
  ozpi_buf call;
  ozpi_buf annots;
} ozpi_step;

typedef struct ozpi_ctx {
  ozpi_arena ar;
  ozp_limits lim;
  ozpi_diag *diags;
  uint32_t ndiags, capdiags;
  ozpi_step *steps;
  uint32_t nsteps, capsteps;
  ozpi_decl *decls;
  uint32_t ndecls, capdecls;
  ozpi_expansion *exps;
  uint32_t nexps, capexps;
  uint64_t fuel, nodes;
  uint32_t maxdepth;
  bool stop;
  bool oom;
  const ozpi_node *annot_node;
  uint32_t annot_step;
  bool let_err;
  void *bind;
  uint32_t nbind, capbind;
  void *vstack;
  uint32_t nv, capv;
} ozpi_ctx;

bool ozpi_grow(ozpi_ctx *c, void **arr, uint32_t *cap, size_t elem, uint32_t need);
void *ozpi_new(ozpi_ctx *c, size_t size);
bool ozpi_str_eq(ozpi_str a, ozpi_str b);
bool ozpi_str_eqz(ozpi_str a, const char *z);
ozpi_str ozpi_strz(const char *z);
ozpi_str ozpi_str_dup(ozpi_ctx *c, const char *p, size_t len);

void ozpi_diag_add(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, const ozpi_str *args, uint32_t nargs);
void ozpi_diag0(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp);
void ozpi_diag1(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a);
void ozpi_diag2(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a, ozpi_str b);
void ozpi_diag3(ozpi_ctx *c, ozpi_diag_id id, ozpi_span span, uint32_t exp, ozpi_str a, ozpi_str b, ozpi_str d);
void ozpi_oom(ozpi_ctx *c);

bool ozpi_buf_put(ozpi_ctx *c, ozpi_buf *b, const char *p, size_t len);
bool ozpi_buf_puts(ozpi_ctx *c, ozpi_buf *b, const char *z);
bool ozpi_buf_putu(ozpi_ctx *c, ozpi_buf *b, uint64_t v);
bool ozpi_buf_puti(ozpi_ctx *c, ozpi_buf *b, int64_t v);
bool ozpi_buf_putrat(ozpi_ctx *c, ozpi_buf *b, ozpi_rat r);
ozpi_str ozpi_buf_str(ozpi_buf *b);
ozpi_str ozpi_fmt_u(ozpi_ctx *c, uint64_t v);

typedef struct ozpi_parse_opts {
  uint32_t first_line;
  bool params_mode;
} ozpi_parse_opts;

bool ozpi_check_utf8(const char *src, size_t len, ozpi_pos *bad);
void ozpi_parse_file(ozpi_ctx *c, const char *src, size_t len);
ozpi_node *ozpi_parse_expr_text(ozpi_ctx *c, const char *src, size_t len, ozpi_span *where);
void ozpi_check_macros(ozpi_ctx *c);
void ozpi_evaluate(ozpi_ctx *c, const char *entry, const char *params);
bool ozpi_put_escaped(ozpi_ctx *c, ozpi_buf *b, ozpi_str s);

#endif
