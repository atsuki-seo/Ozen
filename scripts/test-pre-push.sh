#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/.." && pwd)
mkdir -p "$top/build"
work=$(mktemp -d "$top/build/test-pre-push.XXXX")
trap 'rm -rf "$work"' EXIT

nr=146819854+atsuki-seo@users.noreply.github.com
export HOME="$work/home" GIT_CONFIG_NOSYSTEM=1
mkdir -p "$HOME"

cases=0
failed=0

git init -q --bare "$work/remote.git"
git init -q -b main "$work/clone"
mkdir -p "$work/clone/scripts" "$work/clone/.githooks"
cp "$here/goals.sh" "$work/clone/scripts/goals.sh"
cp -p "$top/.githooks/pre-push" "$work/clone/.githooks/pre-push"
git -C "$work/clone" config core.hooksPath .githooks
git -C "$work/clone" config user.name t
git -C "$work/clone" config user.email "$nr"
git -C "$work/clone" remote add origin "$work/remote.git"
git -C "$work/clone" add -A
git -C "$work/clone" commit -q -m base

cases=$((cases + 1))
if ! git -C "$work/clone" push -q origin main >/dev/null 2>&1; then
  echo "FAIL noreply-push: a push of a noreply commit was refused" >&2
  failed=$((failed + 1))
fi
before=$(git -C "$work/remote.git" rev-parse -q --verify main || true)

echo x > "$work/clone/x.txt"
git -C "$work/clone" add x.txt
git -C "$work/clone" -c user.email=someone@example.org commit -q -m other
cases=$((cases + 1))
if git -C "$work/clone" push -q origin main >/dev/null 2>&1 ||
   [ "$(git -C "$work/remote.git" rev-parse -q --verify main || true)" != "$before" ]; then
  echo "FAIL other-address-push: a push of a commit with another address went through" >&2
  failed=$((failed + 1))
fi

echo "pre-push: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
