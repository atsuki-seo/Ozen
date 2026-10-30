#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 [--root DIR] --list | GOAL-ID..." >&2
  exit 2
}

root=""
ids=()
list_only=0
while [ $# -gt 0 ]; do
  case "$1" in
    --root) [ $# -ge 2 ] || usage; root="$2"; shift 2 ;;
    --list) list_only=1; shift ;;
    -*) usage ;;
    *) ids+=("$1"); shift ;;
  esac
done
if [ -z "$root" ]; then
  root=$(git rev-parse --show-toplevel)
fi
cd "$root" || exit 2

R=plan-start..HEAD
CCFGS="core-gcc-debug-san core-clang-debug-san core-gcc-release core-clang-release"
PLAN=plans/ozen-roadmap.md
RF=refresh-start..HEAD
PLAN_RF=plans/ozen-syntax-refresh.md
RF_N0=363
NOREPLY=146819854+atsuki-seo@users.noreply.github.com

fct_source() {
  local f
  f=$(sed -nE 's/^- File: ([^ ]+\.xlsx)$/\1/p' data/mext-fct-2023/README.md 2>/dev/null | head -n 1)
  printf 'data/mext-fct-2023/source/%s\n' "$f"
}

checks=0
fails=0

pass() { checks=$((checks + 1)); printf 'ok    %s\n' "$1"; }
fail() { checks=$((checks + 1)); fails=$((fails + 1)); printf 'FAIL  %s\n' "$1"; }

want_re() {
  local label=$1 re=$2 text=$3 line n=0 bad=0
  while IFS= read -r line; do
    [ -n "$line" ] || continue
    n=$((n + 1))
    [[ $line =~ $re ]] || bad=1
  done <<< "$text"
  if [ "$n" -gt 0 ] && [ "$bad" -eq 0 ]; then
    pass "$label: $(tail -n 1 <<< "$text")"
  else
    fail "$label: got '$(tr '\n' ';' <<< "$text")', want /$re/"
  fi
}

want_eq() {
  if [ "$3" = "$2" ]; then pass "$1: $3"; else fail "$1: got '$3', want '$2'"; fi
}

want_ge1() {
  if [[ $2 =~ ^[0-9]+$ ]] && [ "$2" -ge 1 ]; then pass "$1: $2"; else fail "$1: got '$2', want >= 1"; fi
}

want_ok() {
  local label=$1
  shift
  if "$@" >/dev/null 2>&1; then pass "$label"; else fail "$label: exit $?"; fi
}

last_num() { grep -oE '[0-9]+' <<< "$1" | sed -n "${2}p"; }


goal_G_REPO_SPDX() {
  local out n
  out=$(scripts/check-spdx.sh 2>&1 | tail -n 1)
  want_re "spdx" '^spdx: [0-9]+ files checked, 0 missing, 0 wrong$' "$out"
  n=$(scripts/check-spdx.sh --list | wc -l)
  want_eq "spdx --list count" "$(last_num "$out" 1)" "$n"
}

goal_G_REPO_LICENSE_TEXT() {
  want_ge1 "MIT permission" "$(grep -c 'Permission is hereby granted, free of charge' LICENSE-MIT)"
  want_ge1 "MIT copyright" "$(grep -c 'Copyright (c) 2026 atsuki.seo' LICENSE-MIT)"
  want_eq "MIT placeholders" 0 "$(grep -E '<year>|<copyright holders>' LICENSE-MIT | wc -l)"
  want_ge1 "Apache version" "$(grep -c 'Version 2.0, January 2004' LICENSE-APACHE)"
  want_ge1 "CC BY title" "$(grep -c 'Attribution 4.0 International' LICENSE-CC-BY-4.0)"
}

goal_G_REPO_README_LICENSE() {
  want_ge1 "code row" "$(grep -cF '| `proto/`, `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |' README.md)"
  want_ge1 "docs row" "$(grep -cF '| `docs/`, `plans/`, `spec/samples/` | CC-BY-4.0 |' README.md)"
  want_ge1 "output sentence" "$(grep -cF 'Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.' README.md)"
}

goal_G_REPO_NOTICE() {
  local r=data/mext-fct-2023/README.md
  want_ge1 "NOTICE attribution" "$(grep -c '日本食品標準成分表（八訂）増補2023年から引用' NOTICE)"
  want_ge1 "README attribution" "$(grep -c '日本食品標準成分表（八訂）増補2023年から引用' $r)"
  want_ge1 "Source URL" "$(grep -cE '^- Source URL: https://www\.mext\.go\.jp/' $r)"
  want_ge1 "Retrieved" "$(grep -cE '^- Retrieved: 20[0-9]{2}-[0-9]{2}-[0-9]{2}$' $r)"
  want_ge1 "File" "$(grep -cE '^- File: [^ ]+\.xlsx$' $r)"
  want_ge1 "SHA-256" "$(grep -cE '^- SHA-256: [0-9a-f]{64}$' $r)"
}

goal_G_REPO_TOOLS() {
  local out n
  out=$(scripts/check-tools.sh 2>&1 | tail -n 1)
  want_re "tools" '^tools: [0-9]+ checked, 0 mismatched$' "$out"
  n=$(awk '$0 !~ /^[[:space:]]*(#|$)/ && ($3 == "" || $3 == "linux")' toolchain.txt | wc -l)
  want_eq "tools count" "$n" "$(last_num "$out" 1)"
}

goal_G_REPO_SELFTESTS() {
  want_re "selftests" '^selftests: [0-9]+ scripts, 0 without test, 0 failed$' "$(scripts/check-selftests.sh 2>&1 | tail -n 1)"
}

goal_G_REPO_RED_FIRST() {
  local out
  out=$(scripts/check-red-first.sh "$R" 2>&1 | tail -n 1)
  want_re "red-first" '^red-first: [1-9][0-9]* commits, 0 not red$' "$out"
}

goal_G_VOCAB_SOURCES() {
  want_re "sources" '^sources: [0-9]+ primary, [0-9]+ provisional, 0 missing fields, 0 quote mismatches, 0 provisional without search(, 0 pdf in repo)?$' \
    "$(python3 -B tools/vocabcheck/sources.py spec/vocab spec/sources/search-log.yaml 2>&1 | tail -n 1)"
  want_eq "pdf in repo" 0 "$(git ls-files '*.pdf' | wc -l)"
}

goal_G_VOCAB_PRIORWORK() {
  want_re "prior-work" '^prior-work: [0-9]+ elements from 4 sources, 0 unmapped, 0 without page$' \
    "$(python3 -B tools/vocabcheck/priorwork.py spec/notes/prior-work-elements.yaml spec/notes/prior-work.md 2>&1 | tail -n 1)"
}

goal_G_VOCAB_SCHEMA() {
  want_re "schema" '^schema: 11 files, 0 errors$' "$(python3 -B tools/vocabcheck/vocabcheck.py schema 2>&1 | tail -n 1)"
  want_eq "vocab files" "actions.yaml aliases.yaml axes.yaml capabilities.yaml dimensions.yaml dirt.yaml ingredients.yaml sources.yaml traits.yaml units.yaml vessels.yaml " \
    "$(ls spec/vocab | tr '\n' ' ')"
}

goal_G_VOCAB_CHECK() {
  local out st
  out=$(python3 -B tools/vocabcheck/vocabcheck.py check 2>&1)
  st=$?
  want_re "check" '^check: 0 errors' "$(tail -n 1 <<< "$out")"
  want_eq "check exit" 0 "$st"
}

goal_G_VOCAB_LISTS() {
  want_re "lists" '^lists: [0-9]+ listed, 0 missing; values: [0-9]+ checked, 0 mismatched; relations: [0-9]+ listed, 0 mismatched$' \
    "$(python3 -B tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml 2>&1 | tail -n 1)"
}

goal_G_VOCAB_LISTS_COPY() {
  want_re "copy" '^copy: [0-9]+ identifiers, 0 differences$' \
    "$(python3 -B scripts/check-lists-copy.py $PLAN spec/coverage/concept-lists.yaml 2>&1 | tail -n 1)"
}

goal_G_VOCAB_SAMPLE() {
  local out k
  out=$(python3 -B tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml 2>&1 | tail -n 1)
  want_re "sample" '^sample: 0 errors; expectations: [0-9]+ checked, 0 failed$' "$out"
  k=$(sed -nE 's/^- サンプルの `expect` の葉の数 k（P2-3）: ([0-9]+).*/\1/p' $PLAN | head -n 1)
  want_eq "expectations = k in 9.2" "$k" "$(last_num "$out" 2)"
}

goal_G_VOCAB_TESTS() {
  local out ran counts fx py
  out=$(python3 -B -m unittest discover -s tools -t . -p 'test_*.py' -v 2>&1 | tail -n 3)
  ran=$(sed -nE 's/^Ran ([0-9]+) tests?.*/\1/p' <<< "$out")
  want_re "unittest OK" '^OK' "$(tail -n 1 <<< "$out")"
  counts=$(scripts/check-plan-tests.sh $PLAN 2>/dev/null | grep -E '^golden=')
  fx=$(sed -nE 's/.* fx=([0-9]+).*/\1/p' <<< "$counts")
  py=$(sed -nE 's/.* py=([0-9]+).*/\1/p' <<< "$counts")
  want_eq "Ran = fx + py" "$((fx + py))" "$ran"
}

goal_G_VOCAB_READONLY() {
  local s0 h0 c
  if [ -n "$(git status --porcelain -- spec tools data)" ]; then
    fail "readonly: spec, tools, or data has uncommitted changes before the run"
    return
  fi
  s0=$(git status --porcelain --ignored -- spec tools data)
  h0=$(git ls-files -z | xargs -0 sha256sum | sha256sum)
  for c in schema check; do python3 tools/vocabcheck/vocabcheck.py $c >/dev/null 2>&1; done
  python3 tools/vocabcheck/vocabcheck.py lists spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml >/dev/null 2>&1
  python3 tools/vocabcheck/vocabcheck.py sample spec/samples/curryRice.yaml >/dev/null 2>&1
  if [ "$s0" = "$(git status --porcelain --ignored -- spec tools data)" ] && [ "$h0" = "$(git ls-files -z | xargs -0 sha256sum | sha256sum)" ]; then
    pass "readonly: no changes"
  else
    fail "readonly: the checker changed files"
  fi
}

goal_G_VOCAB_FCT() {
  want_re "fct" '^fct: [0-9]+ rows, 0 mismatches; sha256 ok; order ok; refs ok; headers ok$' \
    "$(python3 -B tools/fct/extract.py --check --xlsx "$(fct_source)" 2>&1 | tail -n 1)"
}

goal_G_VOCAB_FCT_STABLE() {
  want_ok "fct extract" python3 -B tools/fct/extract.py --xlsx "$(fct_source)"
  want_eq "fct output unchanged" "" "$(git status --porcelain -- data/mext-fct-2023)"
}

goal_G_VOCAB_EXPECT_INDEPENDENT() {
  want_re "separation" '^separation: [1-9][0-9]* commits, 0 mixed$' \
    "$(scripts/check-commit-separation.sh "$R" '^spec/(fixtures|samples)/' '^tools/vocabcheck/' 2>&1 | tail -n 1)"
}

nofloat() {
  local inc=$1 src=$2 hdrs=$3 f
  if ( for f in $src/*.c; do gcc -std=c2x -O2 -mgeneral-regs-only $inc -c "$f" -o /dev/null || exit 1; done ) >/dev/null 2>&1; then
    pass "nofloat compile"
  else
    fail "nofloat compile"
  fi
  want_eq "float/double tokens" 0 "$(for f in $hdrs; do gcc -x c -fpreprocessed -dD -E -P "$f" 2>/dev/null; done | grep -wE 'float|double' | wc -l)"
}

goal_G_PLAN_TESTS_EXIST() {
  want_re "plan-tests" '^plan-tests: [0-9]+ names, 0 missing$' "$(scripts/check-plan-tests.sh $PLAN 2>/dev/null | tail -n 1)"
}

goal_G_PLAN_COVERAGE() {
  want_eq "S items without coverage rows" "" \
    "$(diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
            <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u))"
  want_eq "handoff items without reason" 0 \
    "$(sed -n '/^## 8\./,/^## 9\./p' $PLAN | grep '^- ' | grep -v '機械で決められない理由:' | wc -l)"
}

goal_G_REPO_NOREPLY() {
  want_eq "commit emails" "$NOREPLY " "$(git log --all --format='%ae%n%ce' | sort -u | tr '\n' ' ')"
  want_eq "local user.email" "$NOREPLY" "$(git config --local user.email)"
  want_eq "personal email in tracked files" 0 "$(git grep -nF "$(git config --global user.email)" -- . | wc -l)"
}

goal_G_REPO_CLAUDEMD() {
  want_ge1 "docs/ rule" "$(grep -c 'docs/' CLAUDE.md)"
  want_ge1 "Japanese rule" "$(grep -ciE 'japanese|日本語' CLAUDE.md)"
}



goal_G_REPO_VERSION() {
  local v
  v=$(cat VERSION)
  want_eq "ozen --version" "ozen $v" "$(build/core-gcc-release/ozen --version 2>&1)"
  want_ge1 "stdlib version" "$(grep -cE "^ *version: *\"$v\"" stdlib/package.ozen)"
  want_ge1 "plan version" "$(grep -cF "版: $v" $PLAN)"
}

goal_G_CORE_BUILD() {
  want_eq "core-build" "core-build: 4 configs, 0 failed" "$(scripts/build-core.sh 2>&1 | tail -n 1)"
}

goal_G_CORE_FLAGS() {
  local c
  for c in $CCFGS; do
    want_re "flags $c" '^flags: [0-9]+ files, 0 missing$' \
      "$(python3 -B scripts/check-flags.py --std-gcc c2x --std-clang c23 --extra -Wswitch-enum build/$c/compile_commands.json 2>&1 | tail -n 1)"
  done
}

goal_G_CORE_C23_SUBSET() {
  want_re "c23" '^c23: 12 allowed probes ok on [0-9]+ toolchains, 0 banned uses$' "$(scripts/check-c23-subset.sh core/src cli/src 2>&1 | tail -n 1)"
}

goal_G_CORE_UNIT() {
  local c n
  n=$(ctest --test-dir build/core-gcc-debug-san -N 2>/dev/null | sed -nE 's/^Total Tests: ([0-9]+)$/\1/p')
  for c in $CCFGS; do
    want_eq "unit $c" "100% tests passed, 0 tests failed out of $n" \
      "$(ctest --test-dir build/$c --output-on-failure 2>&1 | grep -E '^[0-9]+% tests passed')"
  done
}

golden_entries() {
  scripts/golden-entries.sh tests/golden | wc -l
}

goal_G_CORE_GOLDEN() {
  local c n
  n=$(golden_entries)
  for c in $CCFGS; do
    want_eq "golden $c" "golden: $n passed, 0 failed, 0 timed out" "$(scripts/run-golden.sh "build/$c/ozen" tests/golden 2>&1 | tail -n 1)"
  done
}

goal_G_CORE_GRAMMAR() {
  want_re "grammar" '^grammar: [0-9]+ rules, 0 not covered$' \
    "$(python3 -B scripts/check-grammar-coverage.py --ozen build/core-gcc-release/ozen core/grammar.ebnf tests/golden 2>&1 | tail -n 1)"
}

goal_G_CORE_PURE() {
  want_eq "pure" "pure: 0 disallowed symbols, 0 disallowed includes" \
    "$(scripts/check-pure.sh core/src build/core-gcc-release/libozen.a build/core-clang-release/libozen.a 2>&1 | tail -n 1)"
}

goal_G_CORE_NOGLOBAL() {
  want_re "noglobal" '^noglobal: [0-9]+ objects, 0 writable sections$' \
    "$(python3 -B scripts/check-noglobal.py build/core-gcc-release/libozen.a build/core-clang-release/libozen.a 2>&1 | tail -n 1)"
}

goal_G_CORE_NOFLOAT() {
  nofloat "-Icore/include -Icore/src" core/src "core/src/*.c core/src/*.h core/include/*.h"
}

goal_G_CORE_NOINT128() {
  want_eq "int128" 0 "$(git grep -nwE '__int128|__uint128_t|_BitInt' -- core cli | wc -l)"
}

goal_G_CORE_ARENA_ACCESS() {
  want_eq "arena structs outside arena.c" 0 "$(git grep -nE 'struct ozni_arena[a-z_]*[[:space:]]*\{' -- core ':!core/src/arena.c' | wc -l)"
  want_eq "index static_assert" 1 "$(git grep -cF 'static_assert(sizeof(ozni_index) == 4' -- core/src/arena.h | sed 's/.*://')"
}

goal_G_CORE_OOM() {
  want_re "oom" '^oom: [0-9]+ files, [0-9]+ points, 0 failures$' "$(build/core-gcc-debug-san/ozen_oom_test tests/golden/*/*.ozen 2>&1 | tail -n 1)"
}

goal_G_CORE_DIAG_REGISTRY() {
  local out n
  out=$(scripts/check-diag-registry.sh 2>&1 | tail -n 1)
  want_re "diag" '^diag: [0-9]+ codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 0 unlisted, 0 unused$' "$out"
  n=$(grep -cvE '^[[:space:]]*(#|$)' core/tests/diag-names.txt)
  want_eq "diag codes = names" "$n" "$(last_num "$out" 1)"
}

goal_G_CORE_DIAG_APPEND() {
  want_eq "deleted registry lines" 0 "$(git log --format= -p "$R" -- core/src/diag_codes.def | grep -E '^-[^-]' | wc -l)"
  want_ge1 "registry commits" "$(git rev-list --count "$R" -- core/src/diag_codes.def)"
}

goal_G_CORE_STD_EMBED() {
  want_ok "embedded std = stdlib" cmp <(build/core-gcc-release/ozen debug embedded-std) <(scripts/concat-std.sh stdlib)
}

goal_G_CORE_PROPS() {
  local t
  for t in reindent split_merge meta_ignored monotonic display_only ir_invariant std_copy bundle_roundtrip; do
    want_re "prop $t" "^$t: [0-9]+ cases, 0 (differences|decreases)$" "$(python3 -B tests/props/$t.py --ozen build/core-gcc-release/ozen 2>&1 | tail -n 1)"
  done
}

goal_G_CORE_DETERMINISM() {
  local c
  want_eq "digests" 1 "$(for c in $CCFGS; do scripts/digest.sh "build/$c/ozen" tests/golden samples; scripts/digest.sh "build/$c/ozen" tests/golden samples; done | sort -u | wc -l)"
}

goal_G_CORE_FUZZ() {
  local out
  out=$(scripts/fuzz-core.sh 600 2>&1 | grep -E '^fuzz-')
  want_re "fuzz-core" '^fuzz-[a-z]+: [1-9][0-9]* execs in ([6-9][0-9]{2}|[0-9]{4,}) s, 0 crashes, 0 leaks, 0 timeouts$' "$out"
  want_eq "fuzz artifacts" 0 "$(ls build/fuzz-core/artifacts 2>/dev/null | wc -l)"
}

goal_G_TYPE_SEVERITY_TABLE() {
  want_re "severity" '^severity: [0-9]+ examples, 0 mismatched; categories: [0-9]+ examples, 0 mismatched$' \
    "$(python3 -B scripts/check-severity-table.py ${SEVERITY_PHASE:+--phase $SEVERITY_PHASE} core/tests/severity-examples.txt core/src/diag_codes.def stdlib 2>&1 | tail -n 1)"
}

definitions() {
  build/core-gcc-release/ozen debug definitions > build/definitions.json
}

goal_G_STD_LISTS() {
  definitions
  want_re "lists" '^lists: [0-9]+ listed, 0 missing; values: [0-9]+ checked, 0 mismatched; relations: [0-9]+ listed, 0 mismatched$' \
    "$(python3 -B tools/stdcheck/lists.py spec/coverage/concept-lists.yaml spec/coverage/concept-relations.yaml build/definitions.json 2>&1 | tail -n 1)"
}

goal_G_STD_STATIC() {
  definitions
  want_re "std-static" '^std-static: [0-9]+ definitions, 0 naming, 0 provisional-point, 0 fct-inline, 0 capability-unprovided$' \
    "$(python3 -B tools/stdcheck/static.py build/definitions.json 2>&1 | tail -n 1)"
}

goal_G_STD_PROVENANCE() {
  definitions
  want_re "provenance" '^provenance: [0-9]+ values, 0 without source$' "$(python3 -B tools/stdcheck/provenance.py build/definitions.json 2>&1 | tail -n 1)"
}

goal_G_STD_SOURCES() {
  definitions
  want_re "sources" '^sources: .*0 missing fields, 0 quote mismatches, 0 provisional without search' \
    "$(python3 -B tools/stdcheck/sources.py build/definitions.json stdlib/sources.ozen spec/sources/search-log.yaml 2>&1 | tail -n 1)"
}

goal_G_STD_PHRASES() {
  definitions
  want_re "phrases" '^phrases: [0-9]+ keys, 0 missing in ja, 0 missing in en, 0 unknown args$' \
    "$(node renderers/tools/phrases-check.mjs build/definitions.json phrases 2>&1 | tail -n 1)"
}

goal_G_SCHED_CHECK() {
  scripts/collect-schedules.sh build/core-gcc-release/ozen tests/golden build/results/schedules >/dev/null
  want_re "schedcheck" '^schedcheck: [1-9][0-9]* schedules, 0 violations$' "$(python3 -B tools/schedcheck/schedcheck.py build/results/schedules/*.json 2>&1 | tail -n 1)"
}

goal_G_API_EXPORTS() {
  want_eq "exports" "recipe_build recipe_default_config recipe_free_string " \
    "$(nm -g --defined-only build/core-gcc-release/libozen.a | awk 'NF==3{print $3}' | grep -v '^ozni_' | sort -u | tr '\n' ' ')"
}

goal_G_API_HEADER() {
  local cc std
  for cc in gcc clang; do
    for std in c17 c23; do
      [ $cc = gcc ] && [ $std = c23 ] && std=c2x
      want_ok "header $cc -std=$std" $cc -std=$std -pedantic-errors -Werror -fsyntax-only -x c core/include/ozen.h
    done
  done
}

goal_G_API_SCHEMA() {
  scripts/run-golden.sh build/core-gcc-release/ozen tests/golden --json build/results/json >/dev/null 2>&1
  want_re "json" '^json: [1-9][0-9]* files, 0 invalid$' "$(python3 -B tools/jsoncheck/validate.py schemas build/results/json 2>&1 | tail -n 1)"
}

goal_G_API_SARIF() {
  scripts/run-golden.sh build/core-gcc-release/ozen tests/golden --sarif build/results/sarif >/dev/null 2>&1
  want_re "sarif" '^sarif: [1-9][0-9]* files, 0 invalid$' \
    "$(python3 -B tools/jsoncheck/validate.py --schema schemas/vendor/sarif-2.1.0.json build/results/sarif 2>&1 | tail -n 1)"
}

goal_G_CLI_TESTS() {
  want_re "cli" '^cli: [1-9][0-9]* cases, 0 failed$' "$(scripts/run-cli-tests.sh build/core-gcc-release/ozen tests/cli 2>&1 | tail -n 1)"
}

goal_G_PLAT_LINUX() {
  want_eq "dynamic symbols" "recipe_build recipe_default_config recipe_free_string " \
    "$(nm -D --defined-only build/core-gcc-release/libozen.so | awk 'NF==3{print $3}' | grep -vE '^(_init|_fini)$' | sort | tr '\n' ' ')"
  want_re "swift" '^swift: [1-9][0-9]* inputs, 0 mismatches$' "$(bindings/linux/swift-test.sh build/core-gcc-release 2>&1 | tail -n 1)"
  want_re "jni" '^jni: [1-9][0-9]* inputs, 0 mismatches$' "$(bindings/android/jvm-test.sh build/core-gcc-release 2>&1 | tail -n 1)"
}

goal_G_PLAT_WASM() {
  want_eq "wasm golden" "golden: $(golden_entries) passed, 0 failed, 0 timed out" "$(scripts/run-golden.sh 'node build/core-wasm/ozen.js' tests/golden 2>&1 | tail -n 1)"
  want_re "worker" '^worker: [1-9][0-9]* inputs, 0 mismatches$' "$(bindings/wasm/worker-test.sh 2>&1 | tail -n 1)"
  want_eq "wasm digest" "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" "$(scripts/digest.sh 'node build/core-wasm/ozen.js' tests/golden samples)"
}

ci_run_id() {
  gh run list --workflow ci.yml --commit "$(git rev-parse HEAD)" --json databaseId -q '.[0].databaseId'
}

ci_download() {
  local id=$1
  [ -d build/ci/.run-$id ] && return
  rm -rf build/ci && gh run download "$id" -D build/ci && mkdir -p build/ci/.run-$id
}

goal_G_PLAT_IOS() {
  local id
  id=$(ci_run_id)
  want_eq "ci run" "success $(git rev-parse HEAD)" "$(gh run view "$id" --json conclusion,headSha -q '.conclusion + " " + .headSha')"
  ci_download "$id"
  want_eq "ios digest" "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" "$(cat build/ci/digest-ios/digest.txt 2>/dev/null)"
  want_eq "xcframework slices" 3 "$(unzip -l build/ci/xcframework/Ozen.xcframework.zip | grep -oE '(ios-arm64-simulator|ios-arm64|macos-arm64_x86_64)/' | sort -u | wc -l)"
  want_re "swift macos" '^swift: [1-9][0-9]* inputs, 0 mismatches$' "$(cat build/ci/swift-macos/result.txt 2>/dev/null)"
}

goal_G_PLAT_ANDROID() {
  local id
  id=$(ci_run_id)
  want_eq "ci run" "success $(git rev-parse HEAD)" "$(gh run view "$id" --json conclusion,headSha -q '.conclusion + " " + .headSha')"
  ci_download "$id"
  want_eq "android digest" "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" "$(cat build/ci/digest-android/digest.txt 2>/dev/null)"
  want_eq "android ABIs" "arm64-v8a armeabi-v7a x86 x86_64 " \
    "$(find build/ci/android-libs -name libozen.so | sed 's#.*/android-libs/##; s#/libozen.so##' | sort | tr '\n' ' ')"
}

goal_G_PLAT_CI() {
  local id
  id=$(ci_run_id)
  want_eq "ci jobs" "" "$(diff <(gh run view "$id" --json jobs -q '.jobs[] | .name + " " + .conclusion' | sort) <(sed 's/$/ success/' ci/jobs.txt | sort))"
}

goal_G_REPO_TOOLS_CI() {
  ci_download "$(ci_run_id)"
  want_re "ci-tools" '^ci-tools: [1-9][0-9]* jobs, 0 mismatched$' "$(python3 -B scripts/check-ci-tools.py toolchain.txt build/ci/tools-*/tools.txt 2>&1 | tail -n 1)"
}

goal_G_SAMPLE_CHECK() {
  want_re "samples" '^samples: [1-9][0-9]* samples, [0-9]+ expectations, 0 failed$' \
    "$(python3 -B tests/samples/check.py --ozen build/core-gcc-release/ozen --save build/results/json/samples samples 2>&1 | tail -n 1)"
}

goal_G_REND_TESTS() {
  local out pass_n rtest
  out=$(node --test renderers/test/ 2>&1 | grep -E '^# (pass|fail)')
  want_eq "renderer failures" "# fail 0" "$(grep '^# fail' <<< "$out")"
  pass_n=$(sed -nE 's/^# pass ([0-9]+)$/\1/p' <<< "$out")
  rtest=$(scripts/check-plan-tests.sh $PLAN 2>/dev/null | sed -nE 's/.* rtest=([0-9]+).*/\1/p')
  if [[ $pass_n =~ ^[0-9]+$ ]] && [ "$pass_n" -ge "${rtest:-0}" ]; then pass "renderer pass >= rtest: $pass_n"; else fail "renderer pass '$pass_n' < rtest '$rtest'"; fi
}

goal_G_REND_SELFCONTAINED() {
  want_re "selfcontained" '^selfcontained: [1-9][0-9]* files, 0 external references$' "$(python3 -B tools/htmlcheck/selfcontained.py build/results/html 2>&1 | tail -n 1)"
}

goal_G_REND_BROWSER_SAFE() {
  want_eq "node-only references" 0 \
    "$(git grep -nE "from ['\"](node:[^'\"]*|fs|path|process|child_process|os)(/[^'\"]*)?['\"]|import\\(['\"](node:|fs|path|os)|require\\(|\\bprocess\\." -- renderers/lib | wc -l)"
  want_re "browser" '^browser: [1-9][0-9]* inputs, 0 mismatches$' "$(renderers/tools/browser-test.sh 2>&1 | tail -n 1)"
}

goal_G_REND_VENDOR() {
  want_ok "fluent license" test -f renderers/vendor/fluent-bundle/LICENSE
  want_re "vendor" '^vendor: [1-9][0-9]* files, 0 mismatched$' "$(python3 -B tools/vendorcheck.py renderers/vendor/README.md schemas/vendor/README.md 2>&1 | tail -n 1)"
}

goal_G_REND_HTML() {
  want_re "html" '^html: [0-9]+ files, 0 not well-formed; gantt: [0-9]+ charts, 0 overlaps, 0 out of bounds; form: [0-9]+ inputs, 0 without label, 0 quantity without unit$' \
    "$(python3 -B tools/htmlcheck/htmlcheck.py build/results/html 2>&1 | tail -n 1)"
}

goal_G_REND_PHRASES_RENDER() {
  want_re "render-phrases" '^render-phrases: [1-9][0-9]* samples × 2 languages, 0 fallbacks, 0 fluent errors, 0 unresolved, 0 identifiers$' \
    "$(node renderers/tools/render-all.mjs --langs ja,en build/results/json 2>&1 | tail -n 1)"
}

goal_G_SAMPLE_STD_ONLY() {
  want_eq "declarations in samples" 0 \
    "$(git grep -nE '^[[:space:]]*(local[[:space:]]+)?(ingredient|action|unit|kind|vessel|dirt|trait)[[:space:]]' -- samples | wc -l)"
}

goal_G_BUNDLE_STABLE() {
  want_ok "bundle twice identical" sh -c 'build/core-gcc-release/ozen bundle samples/menuD --output build/b1.ozen && build/core-gcc-release/ozen bundle samples/menuD --output build/b2.ozen && cmp build/b1.ozen build/b2.ozen'
  want_eq "order-by-name golden" "golden: 1 passed, 0 failed, 0 timed out" \
    "$(scripts/run-golden.sh build/core-gcc-release/ozen tests/golden --only bundle/order-by-name 2>&1 | tail -n 1)"
}

not_yet() { fail "$1: script added in $2"; }

goal_G_RF_DECISIONS() {
  want_eq "decided" 17 "$(grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_RF)"
  want_eq "undecided" 0 "$(grep -c '^### D[0-9]*\. .*【未決】' $PLAN_RF)"
  want_eq "decision lines" 17 "$(grep -c '^- \*\*決定:' $PLAN_RF)"
}

goal_G_RF_COVERAGE() {
  want_eq "S items without coverage rows" "" \
    "$(diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RF | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
            <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RF | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u))"
  want_eq "handoff items without reason" 0 \
    "$(sed -n '/^## 8\./,/^## 9\./p' $PLAN_RF | grep '^- ' | grep -v '機械で決められない理由:' | wc -l)"
}

goal_G_RF_PLAN_TESTS() {
  local out m
  want_re "plan-tests" '^plan-tests: [0-9]+ names, 0 missing$' "$(scripts/check-plan-tests.sh $PLAN_RF 2>/dev/null | tail -n 1)"
  out=$(scripts/check-plan-tests.sh --mutations $PLAN_RF 2>/dev/null | tail -n 1)
  m=$(last_num "$out" 1)
  want_eq "mutations" "mutations: $m rows, $m recorded" "$out"
}

goal_G_RF_TAGS() {
  local t
  want_eq "tags" 4 "$(for t in refresh-start proto-final rewrite-start rewrite-end; do git rev-parse -q --verify "refs/tags/$t" >/dev/null && echo ok; done | wc -l)"
}

goal_G_RF_PROTO_GONE() {
  want_eq "tracked prototype files" 0 "$(git ls-files proto .github/workflows/ios-proto.yml | wc -l)"
  want_eq "proto-final is the parent of the deletion" same \
    "$([ "$(git rev-parse -q --verify proto-final^{commit})" = "$(git rev-parse -q --verify "$(git log --diff-filter=D --format=%H -1 -- proto)^")" ] && echo same)"
  want_eq "prototype paths in moved goals" 0 \
    "$(sed -n '/^goal_G_CORE_\(FLAGS\|PURE\|NOGLOBAL\|DIAG_REGISTRY\)()/,/^}/p' scripts/goals.sh | grep -c 'prot[o]/')"
  want_ge1 "proto-final in report header" "$(sed -n '1,10p' docs/prototype-report.md | grep -c 'proto-final')"
  want_eq "prototype paths in scripts and .github" 0 \
    "$(git grep -nE '(^|[^-a-z])prot[o]/' -- scripts .github ':!scripts/check-red-first.sh' | wc -l)"
}

goal_G_RF_NO_PROTO_GOALS() {
  want_eq "G-PROTO goals" 0 "$(all_ids | grep -c '^G-PROTO-')"
}

goal_G_RF_README_NO_PROTO() {
  want_eq "prototype section" 0 "$(grep -c '^## Evaluator prototype' README.md)"
}

goal_G_RF_README_EXAMPLE() {
  want_re "readme" '^readme: [1-9][0-9]* examples, 0 mismatches$' \
    "$(scripts/check-readme-example.sh --readme README.md --ozen build/core-gcc-release/ozen 2>&1 | tail -n 1)"
}

goal_G_RF_README_PIPE() {
  want_ge1 "pipes in README examples" "$(sed -n '/^```ozen/,/^```/p' README.md | grep -c '|>')"
}

goal_G_RF_NO_EMPTY_PARENS() {
  not_yet "check-test-names.py --empty-parens" P3-2
}

goal_G_RF_CONCEPT_PARENS() {
  want_eq "empty parens in concept" 0 \
    "$(sed -n '/^```/,/^```/p' docs/concept.md | grep -cE '\b(recipe|menu) +[A-Za-z_][A-Za-z0-9_]* *\( *\)|\bmake +[A-Za-z_.]+ *\( *\)')"
}

goal_G_RF_FUZZ_DICT() {
  local w
  for w in '"|>"' '"per"' '"serving"'; do
    want_eq "fuzz dict $w" 1 "$(grep -cxF "$w" core/fuzz/ozen.dict)"
  done
}

goal_G_RF_STD_PREHEAT() {
  not_yet "check-std-preheat.py" P7-1
}

goal_G_RF_SAMPLES_UNUSED() {
  local s
  want_eq "unusedValue in samples" 0 \
    "$(for s in samples/*/; do build/core-gcc-release/ozen check --format json "$s" 2>/dev/null; done | grep -o '"name":"unusedValue"' | wc -l)"
}

goal_G_RF_NAMES() {
  not_yet "check-test-names.py" P5-0
}

goal_G_RF_PATHS() {
  want_eq "added, deleted or renamed goldens" 0 \
    "$(git diff --name-status --no-renames rewrite-start..rewrite-end -- tests/golden 2>&1 | grep -v '^M' | wc -l)"
}

goal_G_RF_TYPEARGS_STABLE() {
  want_ok "typeargs unchanged" git diff --exit-code refresh-start rewrite-start -- tests/golden/typeargs tests/golden/decl/alias.out
}

goal_G_RF_SEPARATION() {
  want_re "separation" '^separation: [1-9][0-9]* commits, 0 mixed$' \
    "$(scripts/check-commit-separation.sh "$RF" '^tests/golden/.*\.out$' '^(core/src|core/include|cli/src|stdlib|phrases|schemas|tools/schedcheck|renderers/lib)/' 2>&1 | tail -n 1)"
}

goal_G_RF_EQUIVALENCE() {
  not_yet "check-rewrite-equivalence.py" P5-0
}

goal_G_RF_SAMPLES_PIPE() {
  not_yet "check-test-names.py --samples-pipe" P5-8
}

goal_G_RF_SAMPLES_SAME() {
  not_yet "check-rewrite-equivalence.py --samples" P5-8
}

goal_G_RF_SAMPLES_EXPECT() {
  want_ok "sample expectations unchanged" git diff --exit-code rewrite-start rewrite-end -- 'samples/*/expect.yaml' renderers/test
}

goal_G_RF_LIMIT_PAIRS() {
  not_yet "check-limit-pairs.py" P5-2
}

goal_G_RF_GOLDEN_COUNT() {
  want_eq "golden entries" $((RF_N0 + 66)) "$(scripts/golden-entries.sh tests/golden | wc -l)"
}

goal_G_RF_CONCEPT() {
  want_ge1 "concept 4.4 pipe or per serving" "$(sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md | grep -cE '\|>|per serving')"
  want_ge1 "concept 4.1 unusedValue" "$(sed -n '/^### 4\.1/,/^### 4\.2/p' docs/concept.md | grep -c 'unusedValue')"
  want_ge1 "concept 4.2 preheat" "$(sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -c 'preheat')"
  want_ge1 "concept 4.5 useWithin" "$(sed -n '/^### 4\.5/,/^### 4\.6/p' docs/concept.md | grep -c 'useWithin')"
  want_ge1 "concept 6.1 new codes" "$(sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep -cE 'servingsUnknown|useWithinMissed')"
}

goal_G_RF_CLAUDEMD() {
  want_ge1 "plan named in CLAUDE.md" "$(grep -c 'plans/ozen-syntax-refresh.md' CLAUDE.md)"
}

goal_G_RF_CI_NO_PROTO() {
  want_eq "ios-proto runs on HEAD" 0 \
    "$(gh run list --workflow ios-proto.yml --commit "$(git rev-parse HEAD)" --json databaseId --jq length 2>&1)"
}

goal_G_RF_AUDIT() {
  want_ge1 "audit records" "$(grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_RF)"
}

all_ids() {
  declare -F | sed -nE 's/^declare -f goal_(G_.*)$/\1/p' | tr '_' '-'
}

if [ "$list_only" -eq 1 ]; then
  all_ids
  exit 0
fi
[ ${#ids[@]} -gt 0 ] || usage

status=0
for id in "${ids[@]}"; do
  fn="goal_${id//-/_}"
  if ! declare -F "$fn" >/dev/null; then
    echo "unknown goal: $id" >&2
    exit 2
  fi
  checks=0
  fails=0
  echo "== $id"
  "$fn"
  if [ "$fails" -eq 0 ]; then
    echo "goal $id: pass ($checks checks)"
  else
    echo "goal $id: FAIL ($fails of $checks checks)"
    status=1
  fi
done
exit $status
