#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
for name in std 2book 'my book'; do
  "$ozen" init book --name "$name" > out.txt 2>&1
  want "$?" 2 "init status for '$name'"
  [ -e book ] && fail "created the folder for '$name'"
done
exit 0
