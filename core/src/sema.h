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

[[nodiscard]] bool ozni_sema_run(ozni_project *pr, bool steps, bool requires, ozni_buf *out);

#endif
