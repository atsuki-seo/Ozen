/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#include <stdint.h>

#include "text.h"

bool ozni_check_utf8(const char *src, size_t len, size_t *bad) {
  size_t i = 0;
  while (i < len) {
    unsigned char b = (unsigned char)src[i];
    size_t n = 0;
    uint32_t cp = 0, min = 0;
    if (b < 0x80) {
      n = 1;
    } else if (b >= 0xc2 && b <= 0xdf) {
      n = 2;
      cp = b & 0x1fu;
      min = 0x80;
    } else if (b >= 0xe0 && b <= 0xef) {
      n = 3;
      cp = b & 0x0fu;
      min = 0x800;
    } else if (b >= 0xf0 && b <= 0xf4) {
      n = 4;
      cp = b & 0x07u;
      min = 0x10000;
    }
    bool ok = n > 0 && i + n <= len;
    for (size_t k = 1; ok && k < n; k++) {
      unsigned char cb = (unsigned char)src[i + k];
      ok = (cb & 0xc0u) == 0x80u;
      cp = (cp << 6) | (cb & 0x3fu);
    }
    if (ok && n > 1 && (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))) {
      ok = false;
    }
    if (!ok) {
      *bad = i;
      return false;
    }
    i += n;
  }
  return true;
}
