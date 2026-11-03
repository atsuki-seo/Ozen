#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/.." && pwd)
mkdir -p "$top/build"
wt=$(mktemp -d "$top/build/test-bump-version.XXXX")
rmdir "$wt"
git -C "$top" worktree add -q --detach "$wt" HEAD
trap 'git -C "$top" worktree remove --force "$wt"' EXIT

cases=0
failed=0

check() {
  local name=$1 cond=$2
  cases=$((cases + 1))
  if ! (eval "$cond"); then
    echo "FAIL $name: $cond" >&2
    failed=$((failed + 1))
  fi
}

for bad in 1.2 v1.2.3 1.2.3.4 1.2.x ""; do
  set +e
  "$wt/scripts/bump-version.sh" "$bad" > /dev/null 2>&1
  status=$?
  set -e
  check "bad-argument '$bad'" "[ $status -eq 2 ] && [ -z \"\$(git -C '$wt' status --porcelain)\" ]"
done

"$wt/scripts/bump-version.sh" 0.9.9 > /dev/null
check changed-files "[ \"\$(git -C '$wt' status --porcelain | sort | tr '\n' ' ')\" = ' M VERSION  M stdlib/package.ozen ' ]"
check version-file "[ \"\$(cat '$wt/VERSION')\" = 0.9.9 ]"
check stdlib-version "grep -qx '  version: \"0.9.9\"' '$wt/stdlib/package.ozen'"
check std-version-check "grep -q 'std: \"0.99.0\"' '$wt/tests/golden/bundle/std-version-check.ozen'"

(cd "$wt" && scripts/build-core.sh > /dev/null 2>&1)
for g in G-CORE-GOLDEN G-CLI-TESTS G-REPO-VERSION G-RD-VERSION G-QC-VERSION G-FB-VERSION; do
  check "bumped $g" "'$wt/scripts/goals.sh' --root '$wt' $g > /dev/null 2>&1"
done

echo "bump-version: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
