/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "ozni.h"
#include "text.h"

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

bool ozni_diag_lookup(const char *code, size_t len, ozni_diag_id *out) {
  for (int id = 0; id < OZNI_D_COUNT; id++) {
    if (ozni_strncmp(registry[id].code, code, len) == 0 && registry[id].code[len] == '\0') {
      *out = (ozni_diag_id)id;
      return true;
    }
  }
  return false;
}
