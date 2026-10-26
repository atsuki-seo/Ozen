#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

dir=${1:?usage: $0 STDLIB_DIR}
(cd "$dir" && find . -name '*.ozen' | sed 's#^\./##' | LC_ALL=C sort) | while IFS= read -r f; do
  printf '// file: %s\n' "$f"
  cat "$dir/$f"
done
