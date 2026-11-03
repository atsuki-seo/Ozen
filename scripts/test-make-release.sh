#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
v=$(cat "$root/VERSION")
unset OZEN_RENDERER

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

python3 -B - "$root/README.md" "$work/soup" <<'PY'
import os, re, sys
text = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r"<!-- file: soup\.ozen -->\n```ozen\n(.*?)```.*?<!-- run: (.*?) -->\n```text\n(.*?)```", text, re.S)
os.makedirs(sys.argv[2])
open(os.path.join(sys.argv[2], "soup.ozen"), "w").write(m.group(1))
open(os.path.join(sys.argv[2], "run"), "w").write(m.group(2))
open(os.path.join(sys.argv[2], "want"), "w").write(m.group(3))
PY
run=$(cat "$work/soup/run")

"$here/make-release.sh" linux --out "$work/dist" > /dev/null
tgz="$work/dist/ozen-$v-linux-x86_64.tar.gz"
check linux-name "[ -f '$tgz' ]"
list=$(tar tzf "$tgz" 2>/dev/null || true)
check linux-contents "for p in bin/ozen renderers/cli.mjs phrases/en.ftl reference/ai-guide.md LICENSE-MIT LICENSE-APACHE \
  LICENSE-CC-BY-4.0 NOTICE README.md; do grep -qx \"ozen-$v/\$p\" <<< \"\$list\" || exit 1; done"
check linux-shared-files "[ -z \"\$(comm -3 <(cd '$root' && git ls-files phrases reference | sort) <(grep -oE '^ozen-$v/(phrases|reference)/.+[^/]$' <<< \"\$list\" | sed 's|^ozen-$v/||' | sort))\" ]"
check linux-no-tests "! grep -q '^ozen-$v/renderers/test/' <<< \"\$list\""
mkdir -p "$work/x"
tar xzf "$tgz" -C "$work/x"
check readme-node "grep -q 'needs \`node\`' '$work/x/ozen-$v/README.md'"
check version "[ \"\$('$work/x/ozen-$v/bin/ozen' --version)\" = 'ozen $v' ]"
check render "cd '$work/soup' && [ \"\$('$work/x/ozen-$v/bin/'${run} 2>&1)\" = \"\$(cat want)\" ]"
mkdir -p "$work/elsewhere"
cp "$work/soup/soup.ozen" "$work/elsewhere/"
check render-on-path "cd '$work/elsewhere' && [ \"\$(PATH='$work/x/ozen-$v/bin':\$PATH ${run} 2>&1)\" = \"\$(cat '$work/soup/want')\" ]"

"$here/make-release.sh" wasm --out "$work/dist" > /dev/null
zip="$work/dist/ozen-$v-wasm.zip"
check wasm-name "[ -f '$zip' ]"
mkdir -p "$work/w"
(cd "$work/w" && unzip -q "$zip")
w="$work/w/ozen-$v"
check wasm-files "for f in ozen.mjs worker.js ozen_bridge.js ozen_bridge.wasm node-run.mjs; do [ -f '$w/'\$f ] || exit 1; done"
python3 -c 'import json,sys; print(json.dumps([{"path": "soup.ozen", "text": open(sys.argv[1]).read()}]))' \
  "$work/soup/soup.ozen" > "$work/inputs.json"
check wasm-run "cd '$w' && node node-run.mjs ./ozen_bridge.js '$work/inputs.json' '$work/out.json' &&
  node -e 'const r = JSON.parse(require(\"fs\").readFileSync(process.argv[1], \"utf8\"))[0];
    process.exit(r !== null && JSON.parse(r).outcome.status === \"ok\" ? 0 : 1)' '$work/out.json'"

"$here/make-release.sh" sums --out "$work/dist" > /dev/null
check sums "[ \$(wc -l < '$work/dist/SHA256SUMS') -eq 2 ] && (cd '$work/dist' && sha256sum -c --quiet SHA256SUMS)"

echo "release: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
