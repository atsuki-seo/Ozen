#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
device=${OZEN_ADB_DIR:-/data/local/tmp/ozen}
quote() {
  printf "'%s'" "${1//\'/\'\\\'\'}"
}
rel=$(realpath --relative-to="$root" "$PWD")
cmd="cd $(quote "$device/repo/$rel") && $(quote "$device/ozen")"
for a in "$@"; do
  cmd+=" $(quote "$a")"
done
# adb shell forwards stdin to the device, which would swallow the entry list the golden runner reads in a loop.
exec adb shell "$cmd" < /dev/null
