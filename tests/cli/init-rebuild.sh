#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
root=$(cd "$(dirname "$0")/../.." && pwd)
. "$(dirname "$0")/lib.sh"
mkdir src
cp -r "$root/core" "$root/cli" "$root/stdlib" "$root/bindings" "$root/VERSION" src/
mkdir -p src/reference
cp "$root/reference/agents-template.md" src/reference/
cmake -S src/core -B build -DCMAKE_BUILD_TYPE=Release > build.log 2>&1 || fail "configure: $(tail -n 3 build.log)"
cmake --build build --target ozen -j 4 >> build.log 2>&1 || fail "build: $(tail -n 3 build.log)"
printf '\nThis line was added after the first build.\n' >> src/reference/agents-template.md
cmake --build build --target ozen -j 4 >> build.log 2>&1 || fail "rebuild: $(tail -n 3 build.log)"
build/ozen init book > out.txt 2>&1 || fail "init failed"
grep -qx 'This line was added after the first build.' book/AGENTS.md || fail "the rebuilt ozen kept the old template"
exit 0
