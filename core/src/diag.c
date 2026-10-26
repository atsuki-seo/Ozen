/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "ozni.h"

static const struct {
  const char *code;
  const char *name;
  ozni_severity severity;
} registry[OZNI_D_COUNT] = {
#define OZNI_DIAG(code, cat, name, sev) {#code, #name, OZNI_SEV_##sev},
#include "diag_codes.def"
#undef OZNI_DIAG
};

const char *ozni_diag_code(ozni_diag_id id) { return registry[id].code; }

const char *ozni_diag_name(ozni_diag_id id) { return registry[id].name; }

ozni_severity ozni_diag_default_severity(ozni_diag_id id) { return registry[id].severity; }
