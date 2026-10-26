#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

seconds=${1:-600}
targets=${OZN_FUZZ_TARGETS:-parse expand typecheck}
root=$(cd "$(dirname "$0")/.." && pwd)
dir="$root/build/fuzz-core"
mkdir -p "$dir/artifacts"
cmake -S "$root/core" -B "$dir" -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DOZN_SANITIZE=ON \
  -DOZN_FUZZ=ON >"$dir/configure.log" 2>&1 || { echo "fuzz-core: configure failed" >&2; exit 2; }
rm -f "$dir"/artifacts/*
status=0
for t in $targets; do
  cmake --build "$dir" --target "fuzz_$t" -j "$(nproc)" >"$dir/build-$t.log" 2>&1 ||
    { echo "fuzz-$t: build failed" >&2; status=2; continue; }
  mkdir -p "$dir/corpus-$t" "$dir/seed-$t"
  find "$root/tests/golden" "$root/stdlib" -name '*.ozen' | while IFS= read -r f; do
    cp "$f" "$dir/seed-$t/$(printf '%s' "$f" | sha256sum | cut -c1-16).ozen"
  done
  "$dir/fuzz_$t" -max_total_time="$seconds" -timeout=5 -max_len=65536 -dict="$root/core/fuzz/ozen.dict" \
    -artifact_prefix="$dir/artifacts/$t-" -print_final_stats=1 "$dir/corpus-$t" "$dir/seed-$t" >"$dir/fuzz-$t.log" 2>&1
  done_line=$(grep -E '^Done [0-9]+ runs in [0-9]+ second' "$dir/fuzz-$t.log" | tail -n 1)
  execs=$(sed -nE 's/^Done ([0-9]+) runs in ([0-9]+) second.*/\1/p' <<< "$done_line")
  secs=$(sed -nE 's/^Done ([0-9]+) runs in ([0-9]+) second.*/\2/p' <<< "$done_line")
  crashes=$(find "$dir/artifacts" -name "$t-crash-*" | wc -l)
  leaks=$(find "$dir/artifacts" -name "$t-leak-*" | wc -l)
  timeouts=$(find "$dir/artifacts" -name "$t-timeout-*" | wc -l)
  summary="fuzz-$t: ${execs:-0} execs in ${secs:-0} s, $crashes crashes, $leaks leaks, $timeouts timeouts"
  echo "$summary" > "$dir/summary-$t.txt"
  echo "$summary"
  if [ -z "$execs" ] || [ "$crashes" -ne 0 ] || [ "$leaks" -ne 0 ] || [ "$timeouts" -ne 0 ]; then
    status=1
  fi
done
exit $status
