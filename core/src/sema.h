/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_SEMA_H
#define OZNI_SEMA_H

#include "num.h"
#include "resolve.h"

enum { OZNI_MAX_DIMS = 16 };

typedef struct ozni_dimvec {
  int8_t e[OZNI_MAX_DIMS];
} ozni_dimvec;

typedef struct ozni_qty {
  ozni_ival v;
  ozni_dimvec d;
  bool known;
} ozni_qty;

typedef enum ozni_ir_kind : uint8_t {
  OZNI_IR_IN,
  OZNI_IR_BUY,
  OZNI_IR_OP,
  OZNI_IR_CHOOSE,
  OZNI_IR_PLATE,
  OZNI_IR_WAVE
} ozni_ir_kind;

typedef struct ozni_ir_node {
  ozni_rat time_lo;
  ozni_rat time_hi;
  uint32_t number;
  ozni_index dish;
  ozni_index label;
  ozni_index text;
  ozni_index first_input;
  uint32_t ninputs;
  ozni_index first_req;
  uint32_t nreqs;
  ozni_index first_variant;
  uint32_t nvariants;
  ozni_index wave;
  ozni_index first_after;
  uint32_t nafter;
  ozni_index file;
  ozni_index src;
  ozni_ir_kind kind;
  bool unattended;
  bool provisional;
  uint8_t reserved[1];
} ozni_ir_node;

typedef struct ozni_ir_input {
  ozni_index node;
  uint32_t part;
  ozni_index divisor;
} ozni_ir_input;

typedef struct ozni_ir_req {
  ozni_index text;
  ozni_index file;
  ozni_index type;
  bool heated;
  ozni_rat temp;
} ozni_ir_req;

typedef struct ozni_ir_variant {
  ozni_rat time_lo;
  ozni_rat time_hi;
  ozni_index name;
  ozni_index first_req;
  uint32_t nreqs;
  bool unattended;
  bool provisional;
  uint8_t reserved[2];
} ozni_ir_variant;

typedef struct ozni_ir {
  ozni_table *nodes;
  ozni_table *inputs;
  ozni_table *reqs;
  ozni_table *variants;
  ozni_table *afters;
} ozni_ir;

typedef enum ozni_sema_out : uint8_t {
  OZNI_OUT_NONE,
  OZNI_OUT_STEPS,
  OZNI_OUT_REQUIRES,
  OZNI_OUT_IR,
  OZNI_OUT_SCHEDULE
} ozni_sema_out;

[[nodiscard]] bool ozni_sema_run(ozni_project *pr, ozni_sema_out what, ozni_buf *out, ozni_ir *ir);

#endif
