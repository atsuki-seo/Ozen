# Contributing to Ozen

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
