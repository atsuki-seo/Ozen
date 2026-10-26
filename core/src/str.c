/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include "text.h"

int ozni_strcmp(const char *a, const char *b) {
  while (*a != '\0' && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

int ozni_strncmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i] || a[i] == '\0') {
      return (unsigned char)a[i] - (unsigned char)b[i];
    }
  }
  return 0;
}

const char *ozni_strchr(const char *s, char ch) {
  for (; *s != '\0'; s++) {
    if (*s == ch) {
      return s;
    }
  }
  return ch == '\0' ? s : nullptr;
}

const char *ozni_memchr(const char *s, char ch, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s[i] == ch) {
      return s + i;
    }
  }
  return nullptr;
}

void ozni_copy(char *dst, const char *src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}
