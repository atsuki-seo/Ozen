/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZEN_CLI_SARIF_H
#define OZEN_CLI_SARIF_H

#include <stddef.h>
#include <stdio.h>

bool ozen_write_sarif(FILE *f, const char *json, size_t len);

#endif
