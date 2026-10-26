/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <string.h>

#include "check.h"

void test_diag_registry(void) {
  CHECK(strcmp(ozni_diag_code(OZNI_D_SYN_invalidUtf8), "SYN001") == 0);
  CHECK(strcmp(ozni_diag_name(OZNI_D_MAC_outOfMemory), "outOfMemory") == 0);
  CHECK(ozni_diag_default_severity(OZNI_D_NAM_localOverride) == OZNI_SEV_INFO);
  CHECK(ozni_diag_default_severity(OZNI_D_SYN_unexpectedToken) == OZNI_SEV_ERROR);
  for (int i = 0; i < OZNI_D_COUNT; i++) {
    for (int j = i + 1; j < OZNI_D_COUNT; j++) {
      CHECK(strcmp(ozni_diag_code((ozni_diag_id)i), ozni_diag_code((ozni_diag_id)j)) != 0);
    }
  }
}
