#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-red-first.sh"

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  git -C "$dir" config user.email test@example.invalid
  git -C "$dir" config user.name test
  mkdir -p "$dir/tools/vocabcheck" "$dir/spec/fixtures/vocab-ng"
  : > "$dir/tools/__init__.py"
  : > "$dir/tools/vocabcheck/__init__.py"
  cat > "$dir/tools/vocabcheck/test_vocabcheck.py" <<'EOF'
import glob
import os
import unittest


class Fixtures(unittest.TestCase):
    pass


def make(path):
    def test(self):
        want = open(path).read().strip()
        impl = "tools/vocabcheck/impl.txt"
        got = open(impl).read().strip() if os.path.exists(impl) else ""
        self.assertEqual(want, got)
    return test


for p in glob.glob("spec/fixtures/*/*.yaml"):
    kind, name = p.split("/")[2], os.path.basename(p)[:-5]
    setattr(Fixtures, "test_fx_" + (kind + "_" + name).replace("-", "_"), make(p))
EOF
  git -C "$dir" add -A
  git -C "$dir" commit -q -m base
  git -C "$dir" tag base
  printf '%s\n' "$dir"
}

commit_file() {
  local repo=$1 path=$2 body=$3
  mkdir -p "$(dirname "$repo/$path")"
  printf '%s\n' "$body" > "$repo/$path"
  git -C "$repo" add "$path"
  git -C "$repo" commit -q -m "add $path"
}

expect() {
  local name=$1 repo=$2 want_last=$3 want_status=$4 got got_status
  cases=$((cases + 1))
  set +e
  got=$(cd "$repo" && "$checker" base..HEAD 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

r=$(new_repo)
commit_file "$r" spec/fixtures/vocab-ng/undefined-trait.yaml ok
commit_file "$r" tools/vocabcheck/impl.txt ok
expect expectation-first "$r" "red-first: 1 commits, 0 not red" 0

r=$(new_repo)
commit_file "$r" tools/vocabcheck/impl.txt ok
commit_file "$r" spec/fixtures/vocab-ng/undefined-trait.yaml ok
expect implementation-first "$r" "red-first: 1 commits, 1 not red" 1

r=$(new_repo)
commit_file "$r" tools/vocabcheck/impl.txt ok
commit_file "$r" spec/fixtures/vocab-ng/undefined-trait.yaml ok
sha=$(git -C "$r" rev-parse HEAD)
commit_file "$r" scripts/red-first-exceptions.txt "$sha expectation fixed after a hand-calculation error"
commit_file "$r" spec/fixtures/vocab-ng/other.yaml nope
expect exception-skipped "$r" "red-first: 1 commits, 0 not red" 0

r=$(new_repo)
commit_file "$r" tools/vocabcheck/impl.txt ok
expect no-expectations "$r" "red-first: 0 commits, 0 not red" 1

r=$(new_repo)
commit_file "$r" renderers/test/a.test.mjs "import test from 'node:test'; test('a', () => { throw new Error('red'); });"
expect rtest-red "$r" "red-first: 1 commits, 0 not red" 0

echo "test-check-red-first: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
