#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cases=0
failed=0

mkdir -p "$work/bin"
cat > "$work/bin/npx" <<'FAKE'
#!/usr/bin/env bash
printf '%s|%s\n' "$PWD" "$*" >> "$FAKE_LOG"
case " $* " in
  *" --check "*)
    if [ -f .rulesync/rules/overview.md ] && ! cmp -s .rulesync/rules/overview.md .generated-from; then
      echo "files are not up to date"; exit 1
    fi
    echo "all files are up to date"; exit 0 ;;
  *" generate "*)
    echo "Written rules"
    for p in $FAKE_WRITE; do mkdir -p "$(dirname "$p")"; echo generated > "$p"; echo "    $p"; done
    cp .rulesync/rules/overview.md .generated-from 2>/dev/null || true ;;
esac
FAKE
chmod +x "$work/bin/npx"

new_root() {
  local r
  r=$(mktemp -d "$work/root.XXXX")
  mkdir -p "$r/scripts" "$r/.rulesync/rules"
  cp "$here/rulesync.sh" "$r/scripts/rulesync.sh"
  printf 'node 24.22.2\nrulesync 43.4.2 npx\n' > "$r/toolchain.txt"
  printf 'CLAUDE.md\nAGENTS.md\n' > "$r/scripts/agent-outputs.txt"
  printf 'rule\n' > "$r/.rulesync/rules/overview.md"
  printf '%s\n' "$r"
}

check() {
  local name=$1 cond=$2
  cases=$((cases + 1))
  if ! eval "$cond"; then
    echo "FAIL $name: $cond" >&2
    failed=$((failed + 1))
  fi
}

run() {
  local r=$1
  shift
  set +e
  (cd /tmp && PATH="$work/bin:$PATH" FAKE_LOG="$r/log" FAKE_WRITE="${FAKE_WRITE:-CLAUDE.md AGENTS.md}" \
    "$r/scripts/rulesync.sh" "$@") > "$r/out" 2>&1
  status=$?
  set -e
}

r=$(new_root)
run "$r" generate --targets x
check runs-pinned "[ $status -eq 0 ] && [ \"\$(cat $r/log)\" = '$r|--yes rulesync@43.4.2 generate --targets x' ]"

r=$(new_root)
sed -i '/^rulesync /d' "$r/toolchain.txt"
run "$r" generate
check missing-line "[ $status -eq 2 ] && grep -q toolchain.txt $r/out && [ ! -e $r/log ]"

r=$(new_root)
run "$r" generate --delete
check refuses-delete "[ $status -eq 2 ] && [ ! -e $r/log ]"

r=$(new_root)
run "$r" generate
printf 'edited\n' >> "$r/.rulesync/rules/overview.md"
run "$r" generate --check
check stale-check "[ $status -eq 1 ] && [ ! -e $r/CLAUDE.md.new ]"
run "$r" generate
run "$r" generate --check
check fresh-check "[ $status -eq 0 ]"

r=$(new_root)
FAKE_WRITE="CLAUDE.md AGENTS.md .extra/rule.md" run "$r" generate
check extra-output "[ $status -eq 1 ] && grep -q '.extra/rule.md' $r/out"

r=$(new_root)
FAKE_WRITE="CLAUDE.md" run "$r" generate
check missing-output "[ $status -eq 1 ] && grep -q 'AGENTS.md' $r/out"

echo "rulesync: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
