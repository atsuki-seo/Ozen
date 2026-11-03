# Contributing to Ozen

## Getting started

```bash
scripts/setup.sh
```

checks the tools against [toolchain.txt](toolchain.txt), turns on the pre-push hook in
`.githooks/`, and generates the agent files. It installs nothing; each line says which
tool is missing or has another version. Then build and run the goals:

```bash
scripts/build-core.sh --only core-gcc-release
scripts/goals.sh G-CORE-GOLDEN G-CLI-TESTS
```

`CLAUDE.md`, `AGENTS.md` and the other agent files are generated from `.rulesync/` by
`scripts/rulesync.sh generate`; edit the source, not the generated files.

## Where things are

| Path | What it holds |
| --- | --- |
| `docs/` | The design: [concept](docs/concept.md) and the prototype report |
| `plans/` | The plans, each with its goals in §5, run by `scripts/goals.sh` |
| `reference/` | The reference for writing Ozen, and the guide for AI agents |
| `site/` | The introduction page |
| The folder README files | One per folder: [core](core/README.md), [cli](cli/README.md), [stdlib](stdlib/README.md), [renderers](renderers/README.md), [bindings](bindings/README.md), [spec](spec/README.md), [site](site/README.md) |
| `.rulesync/` | The source of the instructions to AI agents |
| `CLAUDE.md`, `AGENTS.md` | The instructions to AI agents, generated from `.rulesync/` |

## Building

`scripts/build-core.sh` builds the [core](core/README.md) and the `ozen` command in
four configurations: gcc and clang, Debug with ASan and UBSan, and Release. To build
one configuration only:

```bash
scripts/build-core.sh --only core-gcc-release
```

The versions of the tools are listed in [toolchain.txt](toolchain.txt).

## Tests

```bash
scripts/run-golden.sh build/core-gcc-release/ozen tests/golden
```

runs the golden tests. The plan goals, which check the repository as a whole, run with
`scripts/goals.sh GOAL-ID...` (`scripts/goals.sh --list` lists them).

## CI

CI ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs these jobs:

- `linux`: the four configurations and the tests on Linux
- `fuzz`: each fuzz target for 60 seconds
- `wasm`: the native and WebAssembly cores, compared with each other
- `ios`: the XCFramework, called from Swift on macOS and on the iOS simulator
- `android`: the libraries, called through JNI on the Android emulator

The [platform bindings](bindings/README.md) describe what each platform job checks.

`.github/workflows/agents.yml` checks on every push that every commit uses the noreply
address and that the agent files match `.rulesync/`.

## Releases

A release is made from a version tag:

```bash
scripts/bump-version.sh 0.2.0
git commit -am "Release 0.2.0"
git tag v0.2.0
git push origin main v0.2.0
```

`scripts/bump-version.sh` changes `VERSION` and the version of `std` only; golden
expectations write the version as `{{STD_VERSION}}`. A pushed `v*` tag runs
[`.github/workflows/release.yml`](.github/workflows/release.yml): it checks the tag
against `VERSION`, packs the Linux, macOS and WebAssembly archives with
`scripts/make-release.sh`, runs each of them, and publishes them with `SHA256SUMS` on
GitHub Releases.
