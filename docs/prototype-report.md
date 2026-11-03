# Report on the evaluator prototype

> The prototype `proto/` was removed from the repository by the syntax refresh plan (D52 in `plans/ozen-syntax-refresh.md`). The prototype's code, tests and scripts can still be read at the tag `proto-final` (for example `git show proto-final:proto/defects.tsv`, `git worktree add ../ozen-proto proto-final`). The numbers and paths in this report are as of that tag.

This report sums up what we measured for the 3 items of "what the prototype checks" in concept §9, using the prototype in `proto/` (a subset of C23, the level-3 part of the evaluator: `if`, `for`, `repeat`, `macro` and `quote`, and fuel). The numbers are written from `build/results/`, `build/fuzz/summary.txt` and `proto/defects.tsv`, and `proto/scripts/check-report.py` checks them against those files.

Scope of the prototype: the lexer, a parser that recovers from errors, the evaluator (`let`, `if`, `for`, `repeat`, named arguments, annotations, the entry point and `params`), hygienic macros using expansion marks, limits on fuel, depth and generated nodes, rationals and intervals on our own 128-bit integers, and 31 kinds of diagnostics (with expansion history). The production core `core/` is rebuilt in part D as D12 says, and the parts listed in §4 below are moved over from this prototype together with their tests.

## 1. Ease of writing and number of bugs

### Line counts

| Group of files | Files | Lines |
| --- | --- | --- |
| Core | `proto/src/*.c` | 3,082 |
| Internal headers of the core and the diagnostic registry | `proto/src/*.h` `proto/src/*.def` | 301 |
| Public headers | `proto/include/*.h` | 38 |
| Driver | `proto/driver/*.c` | 175 |
| Unit tests | `proto/tests/unit/*` | 345 |
| Allocation failure tests | `proto/tests/oom/*.c` | 147 |
| Rational oracle | `proto/tests/oracle/*.py` | 159 |
| golden inputs | `proto/tests/golden/**/*.ozen` | 2,589 |
| golden expectations | `proto/tests/golden/**/*.out` | 443 |
| Fuzz entry point | `proto/fuzz/*.c` | 28 |
| WASM bridge and Worker | `proto/wasm/*` | 133 |
| Check and build scripts | `proto/scripts/*` | 1,283 |
| Build settings | `proto/CMakeLists.txt` | 89 |

Inside the core, the parser (`parse.c`) is 1,391 lines and the evaluator (`eval.c`) is 1,084 lines; these 2 make up 80% of it. The 128-bit integers (`u128.c`) and the rationals and intervals (`rational.c`) are 208 lines together.

### Defects

`proto/defects.tsv` records each defect we found, with the means that found it and the commit that fixed it.

| Found by | Count |
| --- | --- |
| unit | 1 |
| golden | 3 |
| measurement | 1 |
| goal | 1 |
| Total | 6 |

- The 1 unit case was a wrong expectation in a test (it assumed `(2^63-2)/2 + 1/2` was an integer), not a bug in the code
- The 3 golden cases were: diagnostic arguments coming out empty; not binding the new value after `NAM:rebind`; and the arena refusing every later request after one failed allocation, so `MAC:outOfMemory` could not be reported
- The 1 measurement case was using 420 MB on `repeat 1000000`, because bindings were allocated again in the arena for each scope (we reused the stacks of bindings and arguments and brought it down to 1.5 MB)
- The 1 goal case was the core calling `strlen`, going outside the allowed C library symbols (caught by `G-PROTO-PURE`)

Besides these, there were 4 mistakes in hand-computed golden expectations (the number of brackets in the nesting-depth input, the position of a diagnostic, the end of a range). Because we commit expectations before the implementation (D20), each one was found at the moment it disagreed with the implementation.

### Ease of writing

- One arena with index references, tagged unions with `switch`, and the X-macro diagnostic registry (`diag_codes.def`) were easy to keep as rules. Checking the registry against golden and the name list (`check-diag-registry.sh`) let a machine catch diagnostics that were added but not registered
- The hard parts were building output (the chain `ok = ok && ozpi_buf_puts(...)` appears 15 times in `api.c` alone) and 128-bit arithmetic without `__int128` (D15). Without the rational oracle (20,000 comparisons with `fractions.Fraction`), the second one would be hard to verify
- The compilers replace common idioms with C library calls (`strlen` in gcc, `bcmp` in clang), so we added 2 symbols to the allow list of the purity check. The core source does not call them
- In the mutation check, the ASan and UBSan builds reported an out-of-bounds read and a stack overflow, and the unit tests, golden and allocation failure tests all passed

## 2. Results per environment

There are 109 golden cases. The digest is one sha256 over golden and the outputs (including exit codes) of the 300 fuzz corpus inputs.

| Environment | golden | digest |
| --- | --- | --- |
| `gcc-debug-san` | 109/109 | — |
| `gcc-release` | 109/109 | — |
| `clang-debug-san` | 109/109 | — |
| `clang-release` | 109/109 | — |
| `native` | — | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |
| `wasm` | 109/109 | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |
| `ios` | 109/109 | `9a6479fb12a33b3a666fd137aba318daefe5b05fa3b80034dac4f40843a84de6` |

- `native` is the digest taken with the gcc Release driver. `G-PROTO-DETERMINISM` confirmed that the digests of the 4 builds (gcc and clang × Debug and Release) match twice each
- WASM was built with Emscripten 6.0.13 and run with node. The 2,000 rational oracle cases also had 0 mismatches
- With the JS bridge (`bridge-test.mjs`) and a browser Web Worker (headless Chrome 154), the results of 4 inputs containing Japanese and emoji matched native
- For iOS, on the GitHub Actions macOS 26 runner (Xcode 26.6), we packed the static libraries for device and simulator into `OzenProto.xcframework`, and took golden and the digest on the arm64 simulator (iPhone 17 Pro) (the runner image was `macos26 20261013.0424.1`; one run took about 9 minutes). CMake makes iOS executables into `.app` by default, so we turned off `CMAKE_MACOSX_BUNDLE`; starting with `simctl spawn` sometimes took over 10 seconds, so we set the limit per run to 120 seconds

## 3. Fuzzing

Result of running libFuzzer (clang 17, with ASan and UBSan, 5 seconds per input, inputs up to 65,536 bytes, with a syntax dictionary) for 600 seconds:

`fuzz: 746382 execs in 601 s, 0 crashes, 0 leaks, 0 timeouts`

- This was the run at gate C (P6-4): about 1,241 runs per second, coverage 1,184 (the first run in P5-1 was 653,862 runs, 1,087 per second, coverage 1,177). Before we rebuilt the library with `-fsanitize=fuzzer-no-link`, coverage did not grow past 3
- We cut the 1,170-input corpus grown in P5-1 down to 300, put it in `proto/tests/corpus/`, and used it as the digest input
- We confirmed that removing the end-of-string check makes it crash on the first input (mutation check)
- 600 seconds found 0 problems, which is within a manageable range. But 600 seconds is short, so for the production core we keep running it on the parser and the expander in CI (concept §10)

## 4. What the user decides

### Judgment on the 3 items of concept §9

1. Ease of writing and number of bugs: 6 defects in 3,082 lines of core (1 of them a test mistake). All were found by golden, unit tests, goal checks or measurement; fuzzing and the sanitizers found none
2. Do iOS and WASM give the same results: the digests of native, WASM and the iOS simulator match, and all 109 golden cases passed in every environment
3. Are the problems fuzzing finds manageable: 0 in 600 seconds

Based on this, decide whether to move on to part D with the core kept as a subset of C23 (D13).

### Parts to bring into production (D12)

When we bring them in, we move them into `core/` together with the same tests (P7-0).

- The 128-bit integers and the rationals and intervals (`u128.c`, `rational.c`), with the unit tests `c/u128_*` and `c/rational_*`, and the rational oracle
- The UTF-8 check (`ozpi_check_utf8`) and the golden cases `syntax/utf8-*`
- The arena with ASan poison padding (`arena.c`) and `c/arena_*`
- The X-macro diagnostic registry and `check-diag-registry.sh` (codes are only appended)
- The driver's exit code rules (0, 1, 2) and `test-driver.sh`
- The golden runner and the digest (`run-golden.sh`, `digest.sh`, `OZP_RUN_TIMEOUT`), and allocation failure injection (`ozp_oom_test`)
- The purity, global variable and warning checks (`check-pure.sh`, `check-noglobal.py`, `check-flags.py`, `check-compilers.sh`)
- The fuzz entry point, dictionary and corpus, the checks of the WASM bridge and Worker, and the iOS workflow

### What to fix in the evaluator's design

- Put bindings and arguments from the start on reused stacks held by the context (no allocation per scope)
- Separate allocation failure into "this request failed" and "the arena is unusable", so that what output building needs can still be allocated after a failure
- Let the output buffer remember failures and check once at the end, removing the `ok = ok && ...` chains
- Names are compared as strings (11 comparisons in `eval.c`). As concept §10 says, intern strings and compare them as integers
- Hygiene by expansion marks worked as expected within the range of golden, so use the same method in production
- Make numeric golden expectations with an oracle (`fractions.Fraction`), or check them against one, instead of computing them by hand
