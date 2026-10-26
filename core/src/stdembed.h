/* SPDX-License-Identifier: MIT OR Apache-2.0 */
#ifndef OZNI_STDEMBED_H
#define OZNI_STDEMBED_H

#include <stddef.h>

typedef struct ozni_std_file {
  const char *path;
  const char *text;
  size_t len;
} ozni_std_file;

extern const ozni_std_file ozni_std_files[];
extern const size_t ozni_std_file_count;

#endif
