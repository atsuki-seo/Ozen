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
RD=readme-start..readme-end
PLAN_RD=plans/ozen-readme-compact.md
OZ=build/core-gcc-release/ozen
OLD="git show readme-start:README.md"
QC=qc-start..HEAD
PLAN_QC=plans/ozen-quantity-checks.md
DOCS_RD="README.md CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md"

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
  want_ge1 "code row" "$(grep -cF '| `tools/`, `scripts/`, `spec/vocab/`, `spec/schema/`, `spec/coverage/`, `spec/fixtures/` | MIT OR Apache-2.0 |' README.md)"
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

ci_ignored() {
  awk '/^    paths-ignore:/ { on = 1; next } on && /^      - / { print ":(exclude,glob)" $2; next } !/^ *#/ { on = 0 }' \
    .github/workflows/ci.yml
}

ci_run_id() {
  local base runs sha ignored
  mapfile -t ignored < <(ci_ignored)
  base=$(git log -1 --format=%H HEAD -- . "${ignored[@]}")
  runs=$(gh run list --workflow ci.yml -L 200 --json databaseId,headSha -q '.[] | .headSha + " " + (.databaseId | tostring)')
  for sha in $(git rev-list "$base~1..HEAD"); do
    awk -v s="$sha" '$1 == s { print $2; found = 1; exit } END { exit !found }' <<< "$runs" && return
  done
}

ci_uncovered() {
  local sha ignored
  sha=$(gh run view "$1" --json headSha -q .headSha)
  mapfile -t ignored < <(ci_ignored)
  git diff --name-only "$sha" HEAD -- . "${ignored[@]}" 2>&1
}

ci_download() {
  local id=$1
  [ -d build/ci/.run-$id ] && return
  rm -rf build/ci && gh run download "$id" -D build/ci && mkdir -p build/ci/.run-$id
}

goal_G_PLAT_IOS() {
  local id
  id=$(ci_run_id)
  want_eq "ci run" "success" "$(gh run view "$id" --json conclusion -q .conclusion)"
  want_eq "ci covers HEAD" "" "$(ci_uncovered "$id")"
  ci_download "$id"
  want_eq "ios digest" "$(scripts/digest.sh build/core-gcc-release/ozen tests/golden samples)" "$(cat build/ci/digest-ios/digest.txt 2>/dev/null)"
  want_eq "xcframework slices" 3 "$(unzip -l build/ci/xcframework/Ozen.xcframework.zip | grep -oE '(ios-arm64-simulator|ios-arm64|macos-arm64_x86_64)/' | sort -u | wc -l)"
  want_re "swift macos" '^swift: [1-9][0-9]* inputs, 0 mismatches$' "$(cat build/ci/swift-macos/result.txt 2>/dev/null)"
}

goal_G_PLAT_ANDROID() {
  local id
  id=$(ci_run_id)
  want_eq "ci run" "success" "$(gh run view "$id" --json conclusion -q .conclusion)"
  want_eq "ci covers HEAD" "" "$(ci_uncovered "$id")"
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
  want_re "empty parens" '^empty-parens: [0-9]+ sources, 0 found, 0 unlisted exceptions$' \
    "$(python3 -B scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt \
      tests core/tests/unit bindings/inputs renderers/test/inputs README.md 2>/dev/null | tail -n 1)"
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
  local out
  out=$(python3 -B scripts/check-std-preheat.py --ozen build/core-gcc-release/ozen 2>/dev/null | tail -n 1)
  want_ge1 "oven kinds" "$(sed -nE 's/^oven kinds: ([0-9]+), with preheatTime: [0-9]+$/\1/p' <<< "$out")"
  want_eq "oven kinds with preheatTime" "$(sed -nE 's/^oven kinds: ([0-9]+), .*/\1/p' <<< "$out")" \
    "$(sed -nE 's/^oven kinds: [0-9]+, with preheatTime: ([0-9]+)$/\1/p' <<< "$out")"
}

goal_G_RF_SAMPLES_UNUSED() {
  local s out ozen=$PWD/build/core-gcc-release/ozen
  out=$(for s in samples/*/; do (cd "$s" && "$ozen" check --format json 2>/dev/null); echo; done)
  want_eq "samples checked" "$(ls -d samples/*/ | wc -l)" "$(grep -c '"schemaVersion"' <<< "$out")"
  want_eq "unusedValue in samples" 0 "$(grep -o '"name":"unusedValue"' <<< "$out" | wc -l)"
}

goal_G_RF_NAMES() {
  want_re "names" '^names: [0-9]+ declarations, [0-9]+ bindings, 0 one-letter, 0 unlisted exceptions$' \
    "$(python3 -B scripts/check-test-names.py --exceptions scripts/test-names-exceptions.txt \
      tests core/tests/unit bindings/inputs 2>/dev/null | tail -n 1)"
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

rewrite_head() {
  if git rev-parse -q --verify refs/tags/rewrite-end >/dev/null; then echo rewrite-end; else echo HEAD; fi
}

goal_G_RF_EQUIVALENCE() {
  want_re "equivalence" \
    '^equivalence: [0-9]+ rewritten, 0 changed results, [0-9]+ changed steps; records: [0-9]+ roadmap rows, 0 missing$' \
    "$(python3 -B scripts/check-rewrite-equivalence.py --base rewrite-start --head "$(rewrite_head)" \
      --records "$PLAN_RF" tests/golden 2>/dev/null | tail -n 1)"
}

goal_G_RF_SAMPLES_PIPE() {
  want_re "samples pipe" '^samples-pipe: [1-9][0-9]* samples, 0 without pipe, 0 unlisted exceptions$' \
    "$(python3 -B scripts/check-test-names.py --samples-pipe --exceptions scripts/samples-pipe-exceptions.txt samples 2>/dev/null | tail -n 1)"
}

goal_G_RF_SAMPLES_SAME() {
  want_re "samples same" '^samples-same: [1-9][0-9]* samples, 0 changed ir, 0 changed schedules, 0 changed renders$' \
    "$(python3 -B scripts/check-rewrite-equivalence.py --base rewrite-start --head "$(rewrite_head)" --samples samples 2>/dev/null | tail -n 1)"
}

goal_G_RF_SAMPLES_EXPECT() {
  want_ok "sample expectations unchanged" git diff --exit-code rewrite-start rewrite-end -- 'samples/*/expect.yaml' renderers/test
}

goal_G_RF_LIMIT_PAIRS() {
  want_re "limit pairs" '^limit-pairs: [1-9][0-9]* pairs, 0 not exact$' \
    "$(python3 -B scripts/check-limit-pairs.py --ozen build/core-gcc-release/ozen tests/golden 2>/dev/null | tail -n 1)"
}

readme_start_goldens() {
  local t
  t=$(mktemp -d)
  git archive readme-start tests/golden | tar -x -C "$t"
  (cd "$t" && "$root/scripts/golden-entries.sh" tests/golden)
  rm -rf "$t"
}

goal_G_RF_GOLDEN_COUNT() {
  local entries e
  entries=$(readme_start_goldens)
  want_eq "golden entries at readme-start" $((RF_N0 + 85)) "$(grep -c . <<< "$entries")"
  want_eq "readme-start goldens missing now" 0 \
    "$(while IFS= read -r e; do [ -n "$e" ] && [ ! -e "$e" ] && echo "$e"; done <<< "$entries" | wc -l)"
}

goal_G_RF_CONCEPT() {
  local syntax severity
  syntax=$(sed -n '/^### 4\.4/,/^### 4\.5/p' docs/concept.md)
  severity=$(sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md)
  want_ge1 "concept 4.4 pipe" "$(grep -c '|>' <<< "$syntax")"
  want_ge1 "concept 4.4 per serving" "$(grep -c 'per serving' <<< "$syntax")"
  want_ge1 "concept 4.4 omitted parentheses" "$(grep -c '`()` を省く' <<< "$syntax")"
  want_ge1 "concept 4.1 unusedValue" "$(sed -n '/^### 4\.1/,/^### 4\.2/p' docs/concept.md | grep -c 'unusedValue')"
  want_ge1 "concept 4.2 preheat" "$(sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md | grep -c 'preheat')"
  want_ge1 "concept 4.5 useWithin" "$(sed -n '/^### 4\.5/,/^### 4\.6/p' docs/concept.md | grep -c 'useWithin')"
  want_eq "concept 6.1 warnings" 2 "$(grep '^| 警告' <<< "$severity" | grep -oE 'unusedValue|useWithinMissed' | sort -u | wc -l)"
  want_eq "concept 6.1 errors" 1 "$(grep '^| エラー' <<< "$severity" | grep -c 'servingsUnknown')"
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

rd_sec() { awk -v h="## $1" '/^```/{f=!f} !f&&$0==h{s=1;next} s&&!f&&/^## /{exit} s' README.md; }
rd_blk() { awk -v r="$1" '$0==r{s=1;next} s&&/^```text$/{t=1;next} t&&/^```$/{exit} t'; }
rd_example() { awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' README.md; }

goal_G_RD_DECISIONS() {
  want_eq "decided" 12 "$(grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_RD)"
  want_eq "undecided" 0 "$(grep -c '^### D[0-9]*\. .*【未決】' $PLAN_RD)"
  want_eq "decision lines" 12 "$(grep -c '^- \*\*決定:' $PLAN_RD)"
}

goal_G_RD_COVERAGE() {
  want_eq "S items without coverage rows" "" \
    "$(diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_RD | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
            <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_RD | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u))"
  want_eq "handoff items without reason" 0 \
    "$(sed -n '/^## 8\./,/^## 9\./p' $PLAN_RD | grep '^- ' | grep -v '機械で決められない理由:' | wc -l)"
}

goal_G_RD_PLAN_TESTS() {
  local out m
  out=$(scripts/check-plan-tests.sh --mutations $PLAN_RD 2>/dev/null | tail -n 1)
  m=$(last_num "$out" 1)
  want_eq "mutations" "mutations: $m rows, $m recorded" "$out"
}

goal_G_RD_HEADINGS() {
  want_eq "README headings" "Build Documentation License " \
    "$(awk '/^```/{f=!f;next} !f&&/^## /{sub(/^## /,"");printf "%s ",$0}' README.md)"
}

goal_G_RD_NO_MOVED() {
  local t
  want_eq "level-3 headings outside code" 0 "$(awk '/^```/{f=!f;next} !f&&/^### /' README.md | wc -l)"
  want_eq "plain code blocks" 0 "$(awk '/^```/{ if(!f){f=1; if($0=="```") n++} else f=0 } END{print n+0}' README.md)"
  want_eq "moved terms in README" 0 \
    "$(for t in 'ozen debug' 'ozen bundle' 'ozen unbundle' 'provisionalValue' 'vocabcheck' 'XCFramework' 'DotGothic16' \
         'recipe_build' 'Gantt' 'run-golden'; do
         grep -qF -- "$t" README.md && echo "$t"
       done | wc -l)"
}

goal_G_RD_NO_PLAN_LINKS() {
  want_eq "links into docs/ or plans/" 0 \
    "$(grep -cE '\]\((https://github\.com/atsuki-seo/Ozen/(blob|tree)/[^/)]+/|\./|\.\./|/)?(docs|plans)(/|\)|#)' README.md)"
  want_eq "design and plan file names" 0 \
    "$(grep -cE 'docs/concept\.md|docs/prototype-report\.md|plans/ozen-[a-z-]+\.md' README.md)"
  want_eq "repository URLs into docs/ or plans/" 0 \
    "$(grep -cE 'atsuki-seo/Ozen/(blob|tree)/[^/ )>]+/(docs|plans)\b' README.md)"
  want_eq "reference links into docs/ or plans/" 0 \
    "$(grep -cE '^ *\[[^]]+\]: *(\./|/)?(docs|plans)(/| |$)' README.md)"
}

goal_G_RD_NO_OZENDATE() {
  want_eq "Ozendate lines" 0 "$(grep -sh 'Ozendate' $DOCS_RD | wc -l)"
}

goal_G_RD_INTRO() {
  local intro t
  intro=$(awk 'NR==1{next} /^$/{if(p)exit; next} {p=1; print}' README.md | tr '\n' ' ')
  for t in 'a language and compiler for cooking procedures' 'dimension types' 'state types' 'linear types' 'schedule'; do
    want_ge1 "intro: $t" "$(grep -cF -- "$t" <<< "$intro")"
  done
  want_ge1 "intro: IR" "$(grep -cw 'IR' <<< "$intro")"
  want_re "intro sentences" '^[23]$' "$(grep -oE '[.!?]( |$)' <<< "$intro" | wc -l)"
}

goal_G_RD_EXAMPLE() {
  local ex build
  want_eq "ozen blocks" 1 "$(grep -c '^```ozen$' README.md)"
  want_eq "recipe soup" 1 "$(grep -c '^recipe soup' README.md)"
  want_eq "render run line" 1 "$(grep -cxF '<!-- run: ozen render --kind recipe --lang en soup.ozen -->' README.md)"
  ex=$(grep -n '^```ozen$' README.md | head -n 1 | cut -d: -f1)
  build=$(grep -n '^## Build$' README.md | cut -d: -f1)
  want_eq "example before Build" before "$([ "$ex" -lt "$build" ] 2>/dev/null && echo before)"
  want_eq "readme" "readme: 1 examples, 0 mismatches" \
    "$(scripts/check-readme-example.sh --readme README.md --ozen $OZ 2>&1 | tail -n 1)"
}

goal_G_RD_PIPE_ALL() {
  want_eq "let lines without pipe" 0 \
    "$(rd_example | grep -E '^ *let [A-Za-z]+ = [a-z]+ ' | grep -v '= make ' | grep -vc '|>')"
  want_eq "let lines with a call on a name" 0 \
    "$(rd_example | grep -cE '^ *let [A-Za-z]+ = [a-z]+\([A-Za-z]+[(,]')"
  want_eq "let lines not starting with an ingredient pipe" 0 \
    "$(rd_example | grep -E '^ *let [A-Za-z0-9_]+ = ' | grep -vE '= (make |simmer\()' | grep -vEc '= [a-z][A-Za-z0-9_]* \|>')"
  want_re "pipes" '^([2-9]|[1-9][0-9]+)$' "$(rd_example | grep -o '|>' | wc -l)"
}

goal_G_RD_RENDER_SAME() {
  local new='<!-- run: ozen render --kind recipe --lang en soup.ozen -->'
  want_eq "render differs from the old README" "" \
    "$(diff <($OLD | rd_blk '<!-- run: ozen render --kind recipe --lang en --entry lunch soup.ozen -->' | grep -vE '^(# |4\. )') \
            <(rd_blk "$new" < README.md | grep -v '^# '))"
  want_re "render lines" '^([5-9]|[1-9][0-9]+)$' "$(rd_blk "$new" < README.md | wc -l)"
  want_eq "renderers example differs from the README" "" \
    "$(diff <(rd_blk "$new" < README.md) <(rd_blk "$new" < renderers/README.md))"
  want_eq "renderers recipe differs from the README" "" \
    "$(diff <(rd_example) <(awk '/^```ozen$/{f=1;next} f&&/^```$/{f=0} f' renderers/README.md))"
}

goal_G_RD_BUILD() {
  local b
  b=$(rd_sec Build)
  want_ge1 "build command" "$(grep -cF 'scripts/build-core.sh --only core-gcc-release' <<< "$b")"
  want_ge1 "build config exists" "$(grep -cF 'core-gcc-release' scripts/build-core.sh)"
  want_ge1 "toolchain link" "$(grep -cF '](toolchain.txt)' <<< "$b")"
  want_ge1 "node" "$(grep -cw 'node' <<< "$b")"
}

goal_G_RD_BUILD_RUN() {
  want_eq "core build" "core-build: 1 configs, 0 failed" \
    "$(scripts/build-core.sh --only core-gcc-release 2>&1 | tail -n 1)"
}

goal_G_RD_DOC_LINKS() {
  local d p
  d=$(rd_sec Documentation)
  want_eq "documents without a link" 0 \
    "$(for p in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
         grep -qF "]($p)" <<< "$d" || echo "$p"
       done | wc -l)"
  want_ge1 "introduction page link" "$(grep -cF '](https://atsuki-seo.github.io/Ozen/)' <<< "$d")"
}

goal_G_RD_LICENSE() {
  local lic l p
  lic=$(rd_sec License)
  want_eq "old license table rows" 8 "$($OLD | grep -c '^| ')"
  want_eq "license table rows missing" 0 \
    "$($OLD | grep '^| ' | while IFS= read -r l; do grep -qxF -- "$l" <<< "$lic" || echo "$l"; done | wc -l)"
  want_eq "output rights sentence" 1 \
    "$(grep -cF 'Recipes you write in Ozen and the schedules and IR the compiler produces are yours; the Ozen licenses do not apply to them.' <<< "$lic")"
  want_eq "license file links missing" 0 \
    "$(for p in LICENSE-MIT LICENSE-APACHE LICENSE-CC-BY-4.0; do grep -qF "]($p)" <<< "$lic" || echo "$p"; done | wc -l)"
  want_ge1 "NOTICE link" "$(grep -cF '](NOTICE)' <<< "$lic")"
}

goal_G_RD_NOTICE() {
  want_ge1 "Fluent in NOTICE" "$(grep -c 'Fluent' NOTICE)"
  want_ge1 "SARIF in NOTICE" "$(grep -c 'SARIF' NOTICE)"
}

goal_G_RD_GFM() {
  local tmp
  tmp=$(mktemp -d)
  gh api -X POST /markdown -f mode=gfm -f context=atsuki-seo/Ozen -F text=@README.md > "$tmp/readme.html" 2>/dev/null
  want_eq "h2 headings" 3 "$(grep -o '<h2' "$tmp/readme.html" | wc -l)"
  want_eq "escaped comments" 0 "$(grep -c '&lt;!--' "$tmp/readme.html")"
  rm -rf "$tmp"
}

goal_G_RD_DOC_TITLES() {
  local f
  want_eq "documents without a title" 0 \
    "$(for f in CONTRIBUTING.md cli/README.md core/README.md stdlib/README.md renderers/README.md bindings/README.md spec/README.md site/README.md; do
         head -n 1 "$f" 2>/dev/null | grep -q '^# ' || echo "$f"
       done | wc -l)"
}

goal_G_RD_FACTS() {
  local facts n l f
  facts=$($OLD | awk '/^```/{ if(!f){f=1; n++; plain=($0=="```")} else f=0; next} f&&plain{print n"\t"$0}')
  want_eq "old plain block lines" 11 "$(wc -l <<< "$facts")"
  want_eq "facts missing" 0 \
    "$(while IFS=$'\t' read -r n l; do
         f=$([ "$n" = 1 ] && echo spec/README.md || echo cli/README.md)
         grep -sqxF -- "$l" "$f" || echo "$f: $l"
       done <<< "$facts" | wc -l)"
}

goal_G_RD_DOC_EXAMPLES() {
  want_eq "cli readme" "readme: 2 examples, 0 mismatches" \
    "$(scripts/check-readme-example.sh --readme cli/README.md --ozen $OZ 2>&1 | tail -n 1)"
  want_eq "renderers readme" "readme: 1 examples, 0 mismatches" \
    "$(scripts/check-readme-example.sh --readme renderers/README.md --ozen $OZ 2>&1 | tail -n 1)"
  local r f
  want_eq "run lines missing" 0 \
    "$(while IFS='|' read -r f r; do
         [ "$(grep -cxF "<!-- run: $r -->" "$f")" = 1 ] && grep -qF "\`$r\`" "$f" || echo "$f: $r"
       done <<'EOF' | wc -l
cli/README.md|ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen
cli/README.md|ozen bundle --entry soup
renderers/README.md|ozen render --kind recipe --lang en soup.ozen
EOF
)"
  want_eq "cli outputs differ from the old README" "" \
    "$(for r in 'ozen debug schedule --entry lunch -k kitchen.ozen soup.ozen' 'ozen bundle --entry soup'; do
         diff <($OLD | rd_blk "<!-- run: $r -->") <(rd_blk "<!-- run: $r -->" < cli/README.md)
       done)"
}

rd_content_pairs() {
  cat <<'EOF'
cli/README.md|`-k`
cli/README.md|--options
cli/README.md|`-s`, `-c`, `-t`
cli/README.md|level 0
cli/README.md|level 1
cli/README.md|level 2
cli/README.md|latest start
cli/README.md|inline `module`
cli/README.md|`std` version
cli/README.md|never overwrites
cli/README.md|in seconds
cli/README.md|start..end
cli/README.md|the cook
cli/README.md|the equipment
cli/README.md|except `std`
core/README.md|C23
core/README.md|no I/O
core/README.md|core/include/ozen.h
core/README.md|recipe_build
core/README.md|schemas/
core/README.md|2020-12
core/README.md|SARIF 2.1.0
stdlib/README.md|`std`
stdlib/README.md|embedded
stdlib/README.md|stdlib/units/
stdlib/README.md|`Kitchen`
stdlib/README.md|`Options`
stdlib/README.md|ozen debug definitions
stdlib/README.md|stdlib/sources.ozen
stdlib/README.md|phrases/ja.ftl
stdlib/README.md|phrases/en.ftl
stdlib/README.md|provisionalValue
renderers/README.md|Gantt
renderers/README.md|shopping list
renderers/README.md|print
renderers/README.md|book
renderers/README.md|settings form
renderers/README.md|no build step
renderers/README.md|WebView
renderers/README.md|renderers/tools/bundle.mjs
renderers/README.md|renderers/vendor/fluent-bundle/
renderers/README.md|phrases/<lang>.ftl
renderers/README.md|falls back to English
renderers/README.md|in node and in browsers
renderers/README.md|then the dependencies
renderers/README.md|name made from the identifier
bindings/README.md|Linux
bindings/README.md|WebAssembly
bindings/README.md|iOS
bindings/README.md|Android
bindings/README.md|bindings/linux/
bindings/README.md|bindings/wasm/
bindings/README.md|bindings/swift/
bindings/README.md|bindings/android/
bindings/README.md|Swift
bindings/README.md|Web Worker
bindings/README.md|XCFramework
bindings/README.md|JNI
bindings/README.md|scripts/build-wasm.sh
bindings/README.md|digest
spec/README.md|spec/vocab/
spec/README.md|tools/vocabcheck/
spec/README.md|data/mext-fct-2023/
spec/README.md|tools/fct/extract.py --check --xlsx
spec/README.md|spec/sources/search-log.yaml
spec/README.md|spec/notes/prior-work.md
spec/README.md|four papers
CONTRIBUTING.md|scripts/build-core.sh
CONTRIBUTING.md|ASan
CONTRIBUTING.md|UBSan
CONTRIBUTING.md|scripts/run-golden.sh build/core-gcc-release/ozen tests/golden
CONTRIBUTING.md|scripts/goals.sh
CONTRIBUTING.md|.github/workflows/ci.yml
CONTRIBUTING.md|four configurations
CONTRIBUTING.md|gcc and clang
CONTRIBUTING.md|Release
CONTRIBUTING.md|scripts/build-core.sh --only core-gcc-release
CONTRIBUTING.md|scripts/goals.sh --list
site/README.md|site/index.html
site/README.md|no build step
site/README.md|DotGothic16
site/README.md|.github/workflows/pages.yml
site/README.md|GitHub Pages
site/README.md|footer
site/README.md|by hand
site/README.md|https://atsuki-seo.github.io/Ozen/
EOF
}

goal_G_RD_DOC_CONTENT() {
  local f t
  want_eq "missing file and term pairs" 0 \
    "$(rd_content_pairs | while IFS='|' read -r f t; do grep -sqF -- "$t" "$f" || echo "$f: $t"; done | wc -l)"
}

goal_G_RD_CI_JOBS() {
  local j
  want_ge1 "jobs in ci/jobs.txt" "$(grep -c . ci/jobs.txt)"
  want_eq "jobs missing from CONTRIBUTING.md" 0 \
    "$(while IFS= read -r j; do grep -sqw -- "$j" CONTRIBUTING.md || echo "$j"; done < ci/jobs.txt | wc -l)"
}

goal_G_RD_TERMS() {
  local t
  want_eq "missing terms" 0 \
    "$(for t in 'ozen check' 'ozen build' 'ozen render' 'ozen bundle' 'ozen unbundle' 'ozen debug' '--format' 'sarif' \
         'core/include/ozen.h' 'recipe_build' 'schemas/' 'SARIF 2.1.0' 'scripts/build-core.sh' 'scripts/run-golden.sh' \
         'scripts/build-wasm.sh' 'bindings/wasm/ozen.mjs' 'bindings/wasm/worker.js' 'XCFramework' 'JNI' \
         '.github/workflows/ci.yml' 'renderers/vendor/fluent-bundle/' 'renderers/tools/bundle.mjs' 'phrases/<lang>.ftl' \
         'ozen debug definitions' 'stdlib/sources.ozen' 'provisionalValue' 'spec/sources/search-log.yaml' \
         'spec/notes/prior-work.md' 'tools/fct/extract.py' 'data/mext-fct-2023/' \
         'site/index.html' '.github/workflows/pages.yml' 'https://atsuki-seo.github.io/Ozen/'; do
         grep -sqF -- "$t" $DOCS_RD || echo "$t"
       done | wc -l)"
}

goal_G_RD_LINKS() {
  local f p x
  want_eq "dead links" 0 \
    "$(for f in $DOCS_RD; do
         grep -soE '\]\([^)]+\)' "$f" | sed -E 's/^\]\(//; s/\)$//; s/ .*$//; s/#.*$//' | grep -vE '^(https?:|mailto:|$)' |
           while IFS= read -r p; do
             x=$(realpath -m --relative-to=. "$(dirname "$f")/$p")
             { [ -e "$x" ] && ! git check-ignore -q "$x"; } || echo "$f: $p"
           done
       done | wc -l)"
}

goal_G_RD_NO_EMPTY_PARENS() {
  want_re "empty parens" '^empty-parens: [0-9]+ sources, 0 found, 0 unlisted exceptions$' \
    "$(python3 -B scripts/check-test-names.py --empty-parens --exceptions scripts/empty-parens-exceptions.txt $DOCS_RD 2>/dev/null | tail -n 1)"
}

goal_G_RD_ENGLISH() {
  want_eq "Japanese lines" 0 "$(LC_ALL=C.UTF-8 grep -shP '[\p{Han}\p{Hiragana}\p{Katakana}]' $DOCS_RD | wc -l)"
}

rd_excluded() {
  grep -oE '^- 範囲に入った別の作業のコミット: [0-9a-f]{7,40}' $PLAN_RD | awk '{print $NF}' |
    while IFS= read -r c; do git rev-parse -q --verify "$c^{commit}" || echo "unknown:$c"; done
}

goal_G_RD_SCOPE() {
  local ex c
  want_eq "tag readme-start" ok "$(git rev-parse -q --verify refs/tags/readme-start >/dev/null && echo ok)"
  ex=$(rd_excluded)
  want_eq "excluded commits outside the range" 0 \
    "$(while IFS= read -r c; do [ -n "$c" ] || continue; git rev-list $RD | grep -qxF -- "$c" || echo "$c"; done <<< "$ex" | wc -l)"
  want_eq "files outside scope" 0 \
    "$(git rev-list --no-merges $RD | grep -vxF -e "${ex:-none}" | while IFS= read -r c; do
         git diff-tree --no-commit-id --no-renames --name-only -r "$c"
       done | sort -u | grep -vxF -e README.md -e CONTRIBUTING.md -e cli/README.md -e core/README.md \
         -e stdlib/README.md -e renderers/README.md -e bindings/README.md -e spec/README.md -e site/README.md \
         -e CLAUDE.md -e scripts/goals.sh -e plans/ozen-readme-compact.md | wc -l)"
}

goal_G_RD_ADD_ONLY() {
  want_eq "removed lines in goals.sh and CLAUDE.md" 0 \
    "$(git diff --no-renames $RD -- scripts/goals.sh CLAUDE.md | grep '^-' | grep -vc '^---')"
}

goal_G_RD_VERSION() {
  want_eq "VERSION" 0.1.0 "$(cat VERSION)"
}

goal_G_RD_CLAUDEMD() {
  want_ge1 "plan line in CLAUDE.md" "$(grep -F 'plans/ozen-readme-compact.md' CLAUDE.md | grep -c 'G-RD-.*readme-start')"
}

goal_G_RD_GH_READMES() {
  local d
  want_eq "folders whose GitHub readme differs" 0 \
    "$(for d in cli core stdlib renderers bindings spec site; do
         [ "$(gh api "repos/atsuki-seo/Ozen/readme/$d" --jq .path 2>/dev/null)" = "$d/README.md" ] || echo "$d"
       done | wc -l)"
}

goal_G_RD_AUDIT() {
  want_ge1 "audit records" "$(grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_RD)"
}

goal_G_QC_DECISIONS() {
  want_eq "decided" 19 "$(grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN_QC)"
  want_eq "undecided" 0 "$(grep -c '^### D[0-9]*\. .*【未決】' $PLAN_QC)"
  want_eq "decision lines" 19 "$(grep -c '^- \*\*決定:' $PLAN_QC)"
}

goal_G_QC_COVERAGE() {
  want_eq "S items without coverage rows" "" \
    "$(diff <(sed -n '/^## 4\./,/^## 5\./p' $PLAN_QC | grep -oE '\*\*S[0-9]+\*\*' | tr -d '*' | sort -u) \
            <(sed -n '/^### 5\.3/,/^## 6\./p' $PLAN_QC | grep -oE '^\| S[0-9]+' | tr -d '| ' | sort -u))"
  want_eq "handoff items without reason" 0 \
    "$(sed -n '/^## 8\./,/^## 9\./p' $PLAN_QC | grep '^- ' | grep -v '機械で決められない理由:' | wc -l)"
}

goal_G_QC_PLAN_NAMES() {
  want_re "plan-tests" '^plan-tests: [1-9][0-9]* names, 0 missing$' \
    "$(scripts/check-plan-tests.sh $PLAN_QC 2>/dev/null | tail -n 1)"
}

goal_G_QC_PLAN_TESTS() {
  local out m
  out=$(scripts/check-plan-tests.sh --mutations $PLAN_QC 2>/dev/null | tail -n 1)
  m=$(last_num "$out" 1)
  want_eq "mutations" "mutations: $m rows, $m recorded" "$out"
}

qc_golden_dir() {
  want_eq "$1 golden entries" "$2" "$(scripts/golden-entries.sh "tests/golden/$1" | wc -l)"
  want_eq "$1 golden run" "golden: $2 passed, 0 failed, 0 timed out" \
    "$(scripts/run-golden.sh $OZ "tests/golden/$1" 2>&1 | tail -n 1)"
}

goal_G_QC_GOLDEN_ARGS() {
  qc_golden_dir args 22
}

goal_G_QC_GOLDEN_BOUNDS() {
  qc_golden_dir bounds 55
}

goal_G_QC_GOLDEN_CHANGED() {
  local f
  want_eq "tag qc-start" ok "$(git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok)"
  want_eq "changed expectations not named in the plan" 0 \
    "$(git diff --name-only --diff-filter=M qc-start HEAD -- 'tests/golden/**/*.out' 'renderers/test/**' 'samples/*/expect.yaml' |
       while IFS= read -r f; do grep -qF -- "- 期待を直した golden: \`$f\`" $PLAN_QC || echo "$f"; done | wc -l)"
}

goal_G_QC_PRELUDE() {
  local p=stdlib/prelude.ozen
  want_eq "time ranges" 7 "$(grep -cE '^action (soak|fry|simmer|boil|parboil|chill|rest)\(.*\btime: Time in 0 s<\.\.' $p)"
  want_eq "size range" 1 "$(grep -cE '^action cut\(.*\bsize: Length in 0 mm<\.\. = 3 mm\)' $p)"
  want_eq "temperature ranges" 0 "$(grep -cE 'Temperature\??[[:space:]]+in\b' $p)"
  want_eq "servings range" 1 "$(grep -cE '^  servings: Count\? in 1\.\.$' $p)"
}

goal_G_QC_DIAG() {
  local n021 n011
  want_eq "TYP021 entry" 1 "$(grep -cx 'OZNI_DIAG(TYP021, TYP, nonPositiveAmount, ERROR)' core/src/diag_codes.def)"
  n021=$(grep -n 'TYP021' core/src/diag_codes.def | head -n 1 | cut -d: -f1)
  n011=$(grep -n 'SCH011' core/src/diag_codes.def | head -n 1 | cut -d: -f1)
  want_eq "TYP021 after SCH011" after "$([ -n "$n021" ] && [ "$n021" -gt "$n011" ] && echo after)"
  want_eq "registry" 1 "$(grep -cxF 'TYP:nonPositiveAmount' core/tests/diag-names.txt)"
  want_eq "en phrase" 1 "$(grep -E '^diag-non-positive-amount = ' phrases/en.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }')"
  want_eq "ja phrase" 1 "$(grep -E '^diag-non-positive-amount = ' phrases/ja.ftl | grep -F '{ $arg1 }' | grep -cF '{ $arg2 }')"
  want_eq "en phrase text" 1 \
    "$(grep -cxF 'diag-non-positive-amount = The amount { $arg2 } of { $arg1 } must be more than zero' phrases/en.ftl)"
  want_eq "ja phrase text" 1 "$(grep -cxF 'diag-non-positive-amount = { $arg1 }の量 { $arg2 } は 0 より大きくしてください' phrases/ja.ftl)"
  want_eq "arg kinds" 1 "$(grep -cE "^  nonPositiveAmount: \['name'\]," renderers/lib/names.mjs)"
  want_eq "severity example" 1 "$(grep -cP '\tTYP:nonPositiveAmount\terror\t' core/tests/severity-examples.txt)"
}

goal_G_QC_SCHEMA() {
  local d=build/results/qc-types
  want_eq "minExclusive beside min" "$(grep -c '"min": {' schemas/result.schema.json)" \
    "$(grep -c '"minExclusive"' schemas/result.schema.json)"
  mkdir -p $d && rm -f $d/*.json
  $OZ check --format json --emit types tests/golden/bounds/types-json-exclusive.ozen > $d/exclusive.result.json 2>/dev/null
  want_eq "schema" "json: 1 files, 0 invalid" "$(python3 -B tools/jsoncheck/validate.py schemas $d 2>&1 | tail -n 1)"
}

goal_G_QC_SYNTAX() {
  local ranges
  ranges=$(sed -n '/^### 4\.2/,/^### 4\.3/p' docs/concept.md)
  want_ge1 "grammar type_range" "$(grep -cE '^type_range[[:space:]]*:=' core/grammar.ebnf)"
  want_ge1 "rules.def type_range" "$(grep -cx 'OZNI_RULE(type_range)' core/src/rules.def)"
  want_ge1 "fuzz dict" "$(grep -cxF '"<.."' core/fuzz/ozen.dict)"
  want_ge1 "concept 4.2 example" "$(grep -cF '0 s<..' <<< "$ranges")"
  want_ge1 "concept 4.2 upper end" "$(grep -cF 'a<..b' <<< "$ranges")"
  want_ge1 "concept 4.2 upper end included" "$(grep -cF '上端は含む' <<< "$ranges")"
  want_ge1 "concept 4.2 action arguments" "$(grep -F '範囲外は必ずエラー' <<< "$ranges" | grep -cF '動作の引数')"
  want_ge1 "concept Options servings" "$(grep -cE '^  servings: +Count\? in 1\.\.' docs/concept.md)"
  want_ge1 "concept 6.1 amount" "$(sed -n '/^### 6\.1/,/^## 7\./p' docs/concept.md | grep -cF '0 以下の量')"
}

qc_excluded() {
  grep -oE '^- 範囲に入った別の作業のコミット: [0-9a-f]{7,40}' $PLAN_QC | awk '{print $NF}' |
    while IFS= read -r c; do git rev-parse -q --verify "$c^{commit}" || echo "unknown:$c"; done
}

qc_in_scope() {
  case "$1" in
    core/src/*.c | core/src/*.h | core/src/*.def | core/grammar.ebnf | core/fuzz/ozen.dict | \
    core/tests/diag-names.txt | core/tests/severity-examples.txt | stdlib/prelude.ozen | \
    phrases/en.ftl | phrases/ja.ftl | renderers/lib/names.mjs | renderers/test/* | \
    schemas/result.schema.json | tests/golden/* | docs/concept.md | scripts/goals.sh | CLAUDE.md | \
    scripts/red-first-exceptions.txt | plans/ozen-quantity-checks.md | plans/ozen-syntax-refresh.md | \
    plans/ozen-readme-compact.md) return 0 ;;
  esac
  return 1
}

goal_G_QC_SCOPE() {
  local ex c f
  want_eq "tag qc-start" ok "$(git rev-parse -q --verify refs/tags/qc-start >/dev/null && echo ok)"
  ex=$(qc_excluded)
  want_eq "excluded commits outside the range" 0 \
    "$(while IFS= read -r c; do [ -n "$c" ] || continue; git rev-list $QC | grep -qxF -- "$c" || echo "$c"; done <<< "$ex" | wc -l)"
  want_eq "files outside scope" 0 \
    "$(git rev-list --no-merges $QC | grep -vxF -e "${ex:-none}" | while IFS= read -r c; do
         git diff-tree --no-commit-id --no-renames --name-only -r "$c"
       done | sort -u | while IFS= read -r f; do qc_in_scope "$f" || echo "$f"; done | wc -l)"
}

goal_G_QC_VERSION() {
  want_eq "VERSION" 0.1.0 "$(cat VERSION)"
}

goal_G_QC_CLAUDEMD() {
  want_ge1 "plan line in CLAUDE.md" "$(grep -c 'ozen-quantity-checks.md.*G-QC-.*qc-start' CLAUDE.md)"
}

goal_G_QC_RF_COUNT() {
  want_ge1 "readme-start in G-RF-GOLDEN-COUNT" \
    "$(sed -n '/^goal_G_RF_GOLDEN_COUNT()/,/^}/p' scripts/goals.sh | grep -c 'readme-start')"
  want_ok "G-RF-GOLDEN-COUNT" scripts/goals.sh G-RF-GOLDEN-COUNT
}

goal_G_QC_RD_RANGE() {
  want_eq "readme-end" 52a6ef1 "$(git rev-parse -q --short=7 readme-end 2>/dev/null)"
  want_eq "RD range" 1 "$(grep -cx 'RD=readme-start..readme-end' scripts/goals.sh)"
  want_ok "G-RD-SCOPE and G-RD-ADD-ONLY" scripts/goals.sh G-RD-SCOPE G-RD-ADD-ONLY
}

qc_range_shas() {
  local h
  grep -oE '\b[0-9a-f]{7,40}\b' | sort -u | while IFS= read -r h; do
    git rev-parse -q --verify "$h^{commit}" 2>/dev/null
  done | while IFS= read -r h; do git rev-list $QC | grep -qxF -- "$h" && echo "$h"; done
}

goal_G_QC_ISSUES() {
  local c1 c2
  c1=$(gh issue view 1 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' 2>/dev/null)
  c2=$(gh issue view 2 --repo atsuki-seo/Ozen --json comments -q '.comments[].body' 2>/dev/null)
  want_eq "issue 1" CLOSED "$(gh issue view 1 --repo atsuki-seo/Ozen --json state -q .state 2>&1)"
  want_eq "issue 2" CLOSED "$(gh issue view 2 --repo atsuki-seo/Ozen --json state -q .state 2>&1)"
  want_ge1 "issue 1 comment" "$(grep -c 'TYP001' <<< "$c1")"
  want_ge1 "issue 2 comment" "$(grep -c 'TYP021' <<< "$c2")"
  want_ge1 "issue 2 comment TYP012" "$(grep -c 'TYP012' <<< "$c2")"
  want_ge1 "issue 1 output" "$(grep -cF 'case1.ozen:5:20-5:24' <<< "$c1")"
  want_ge1 "issue 2 output" "$(grep -cF 'case2.ozen:3:12-3:19' <<< "$c2")"
  want_ge1 "issue 2 golden" "$(grep -cF 'bounds/issue2' <<< "$c2")"
  want_ge1 "issue 1 commit" "$(qc_range_shas <<< "$c1" | wc -l)"
  want_ge1 "issue 2 commit" "$(qc_range_shas <<< "$c2" | wc -l)"
}

goal_G_QC_AUDIT() {
  want_ge1 "audit records" "$(grep -cE '^- 網羅の監査: 指摘 [0-9]+ 件、取り込み [0-9]+ 件' $PLAN_QC)"
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
