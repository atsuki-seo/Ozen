/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_TEXT_H
#define OZNI_TEXT_H

#include <stdbool.h>
#include <stddef.h>

bool ozni_check_utf8(const char *src, size_t len, size_t *bad);
int ozni_strcmp(const char *a, const char *b);
int ozni_strncmp(const char *a, const char *b, size_t n);
const char *ozni_strchr(const char *s, char ch);
const char *ozni_memchr(const char *s, char ch, size_t n);
void ozni_copy(char *dst, const char *src, size_t n);

#endif
