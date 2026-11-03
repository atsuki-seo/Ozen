#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cases=0
failed=0

fake="$work/bin"
mkdir -p "$fake"
for t in gcc cmake node python3 xcodebuild; do
  cat > "$fake/$t" <<'FAKE'
#!/usr/bin/env bash
v=$(printenv "FAKE_$(basename "$0" | tr a-z A-Z)")
[ -n "$v" ] || exit 127
case "$(basename "$0")" in
  gcc) echo "$v" ;;
  cmake) echo "cmake version $v" ;;
  node) echo "v$v" ;;
  python3) echo "$v" ;;
  xcodebuild) echo "Xcode $v" ;;
esac
FAKE
  chmod +x "$fake/$t"
done
cat > "$fake/uname" <<'FAKE'
#!/usr/bin/env bash
echo "${FAKE_UNAME:-Linux}"
FAKE
chmod +x "$fake/uname"

new_root() {
  local r
  r=$(mktemp -d "$work/root.XXXX")
  mkdir -p "$r/scripts"
  cp "$here/setup.sh" "$here/check-tools.sh" "$r/scripts/"
  cat > "$r/scripts/rulesync.sh" <<'FAKE'
#!/usr/bin/env bash
cd "$(dirname "$0")/.." || exit 2
[ -z "${FAKE_RULESYNC_FAIL:-}" ] || exit 1
printf 'rules\n' > AGENTS.md
FAKE
  chmod +x "$r/scripts/rulesync.sh"
  printf 'AGENTS.md\n' > "$r/scripts/agent-outputs.txt"
  cat > "$r/toolchain.txt" <<'TC'
gcc 13.3.0
cmake 3.28.3
node 24.22.2
rulesync 43.4.2 npx
python3 3.12.3
emcc 6.0.13
xcode 26.6 macos
ndk 30.0.1 ci
TC
  git -C "$r" init -q
  printf '%s\n' "$r"
}

run() {
  local r=$1
  set +e
  out=$(cd /tmp && PATH="$fake:$PATH" EMSDK=/nonexistent FAKE_GCC="${FAKE_GCC-13.3.0}" FAKE_CMAKE="${FAKE_CMAKE-3.28.3}" \
    FAKE_NODE="${FAKE_NODE-24.22.2}" FAKE_PYTHON3="${FAKE_PYTHON3-3.12.3}" FAKE_XCODEBUILD="${FAKE_XCODEBUILD-}" \
    "$r/scripts/setup.sh" 2>&1)
  status=$?
  set -e
}

check() {
  local name=$1 cond=$2
  cases=$((cases + 1))
  if ! eval "$cond"; then
    echo "FAIL $name: $cond; output: $(tr '\n' ';' <<< "$out") status $status" >&2
    failed=$((failed + 1))
  fi
}

has() { grep -qxF -- "$1" <<< "$out"; }

r=$(new_root)
run "$r"
check linux-lines "has 'ok gcc 13.3.0' && has 'ok cmake 3.28.3' && has 'ok node 24.22.2' && has 'ok python3 3.12.3' && [ \$(wc -l <<< \"\$out\") -eq 10 ]"
check linux-skips "has 'skip rulesync' && has 'skip xcode' && has 'skip ndk'"
check emcc-missing-ok "has 'missing emcc want 6.0.13' && [ $status -eq 0 ]"
check hooks-set "[ \"\$(git -C $r config --local core.hooksPath)\" = .githooks ] && has 'hooksPath: .githooks'"
check generated "has 'agent files: updated' && [ -f $r/AGENTS.md ]"

run "$r"
first=$out
snap=$(cd "$r" && find . -path ./.git -prune -o -type f -print0 | sort -z | xargs -0 sha256sum; git -C "$r" config --local --list)
run "$r"
check second-run "[ \"\$out\" = \"\$first\" ] && has 'agent files: unchanged' &&
  [ \"\$(cd $r && find . -path ./.git -prune -o -type f -print0 | sort -z | xargs -0 sha256sum; git -C $r config --local --list)\" = \"\$snap\" ]"

r=$(new_root)
FAKE_CMAKE=3.20.0 run "$r"
check cmake-differs "has 'differs cmake want 3.28.3 got 3.20.0' && [ $status -eq 1 ]"

r=$(new_root)
FAKE_NODE= run "$r"
check node-missing "has 'missing node want 24.22.2' && [ $status -eq 1 ]"

r=$(new_root)
FAKE_GCC= run "$r"
check gcc-missing "has 'missing gcc want 13.3.0' && [ $status -eq 1 ]"

r=$(new_root)
FAKE_PYTHON3= run "$r"
check python3-missing "has 'missing python3 want 3.12.3' && [ $status -eq 1 ]"

r=$(new_root)
FAKE_UNAME=Darwin FAKE_GCC= FAKE_NODE= run "$r"
check macos-node-missing "has 'missing node want 24.22.2' && [ $status -eq 1 ]"

r=$(new_root)
FAKE_UNAME=Darwin FAKE_GCC= run "$r"
check macos "has 'missing gcc want 13.3.0' && has 'missing xcode want 26.6' && [ $status -eq 0 ]"

r=$(new_root)
git -C "$r" config --local core.hooksPath .husky
run "$r"
check hooks-kept "has 'hooksPath: kept .husky' && [ \"\$(git -C $r config --local core.hooksPath)\" = .husky ]"

r=$(new_root)
FAKE_RULESYNC_FAIL=1 run "$r"
check rulesync-fails "has 'agent files: skipped (rulesync failed)' && [ $status -eq 0 ]"

echo "setup: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
