#!/bin/sh
# SPDX-License-Identifier: MIT OR Apache-2.0
bad=$(nm -g --defined-only "$1" | awk 'NF == 3 { print $3 }' | grep -vE '^(ozni_|recipe_)' || true)
if [ -n "$bad" ]; then
  echo "symbols without the ozni_ prefix:" >&2
  echo "$bad" >&2
  exit 1
fi
