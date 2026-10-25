#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

seconds=${1:-600}
root=$(cd "$(dirname "$0")/../.." && pwd)
dir="$root/build/fuzz"
mkdir -p "$dir/corpus" "$dir/seed" "$dir/artifacts"
cmake -S "$root/proto" -B "$dir" -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DOZP_SANITIZE=ON \
  -DOZP_FUZZ=ON >"$dir/configure.log" 2>&1 || { echo "fuzz: configure failed" >&2; exit 2; }
cmake --build "$dir" --target fuzz_expand -j "$(nproc)" >"$dir/build.log" 2>&1 || { echo "fuzz: build failed" >&2; exit 2; }
rm -f "$dir"/artifacts/*
find "$root/proto/tests/golden" -name '*.ozen' -exec cp {} "$dir/seed/" \;
[ -d "$root/proto/tests/corpus" ] && cp "$root"/proto/tests/corpus/* "$dir/seed/" 2>/dev/null
"$dir/fuzz_expand" -max_total_time="$seconds" -timeout=5 -max_len=65536 -dict="$root/proto/fuzz/ozen.dict" \
  -artifact_prefix="$dir/artifacts/" -print_final_stats=1 "$dir/corpus" "$dir/seed" >"$dir/fuzz.log" 2>&1
done_line=$(grep -E '^Done [0-9]+ runs in [0-9]+ second' "$dir/fuzz.log" | tail -n 1)
execs=$(sed -nE 's/^Done ([0-9]+) runs in ([0-9]+) second.*/\1/p' <<< "$done_line")
secs=$(sed -nE 's/^Done ([0-9]+) runs in ([0-9]+) second.*/\2/p' <<< "$done_line")
crashes=$(find "$dir/artifacts" -name 'crash-*' | wc -l)
leaks=$(find "$dir/artifacts" -name 'leak-*' | wc -l)
timeouts=$(find "$dir/artifacts" -name 'timeout-*' | wc -l)
summary="fuzz: ${execs:-0} execs in ${secs:-0} s, $crashes crashes, $leaks leaks, $timeouts timeouts"
echo "$summary" > "$dir/summary.txt"
echo "$summary"
[ -n "$execs" ] && [ "$crashes" -eq 0 ] && [ "$leaks" -eq 0 ] && [ "$timeouts" -eq 0 ]
