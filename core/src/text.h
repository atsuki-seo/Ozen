/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_TEXT_H
#define OZNI_TEXT_H

#include <stdbool.h>
#include <stddef.h>

bool ozni_check_utf8(const char *src, size_t len, size_t *bad);

#endif
