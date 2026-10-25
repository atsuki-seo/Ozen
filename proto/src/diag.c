/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "ozpi.h"

static const char *const codes[] = {
#define OZPI_DIAG(code, cat, name) #code,
#include "diag_codes.def"
#undef OZPI_DIAG
};

static const char *const names[] = {
#define OZPI_DIAG(code, cat, name) #name,
#include "diag_codes.def"
#undef OZPI_DIAG
};

const char *ozpi_diag_code(ozpi_diag_id id) { return codes[id]; }

const char *ozpi_diag_name(ozpi_diag_id id) { return names[id]; }
