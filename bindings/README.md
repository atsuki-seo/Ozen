# Platform bindings

The same [core](../core/README.md) runs on four platforms:

- **Linux**: linked directly; [`bindings/linux/`](linux/) calls it from Swift.
- **WebAssembly**: built with `scripts/build-wasm.sh`, then
  [`bindings/wasm/ozen.mjs`](wasm/ozen.mjs) runs it in node or in a Web Worker through
  [`bindings/wasm/worker.js`](wasm/worker.js).
- **iOS**: an XCFramework with the Swift wrapper in [`bindings/swift/`](swift/).
- **Android**: JNI in [`bindings/android/`](android/).

Each runs the golden tests and the samples and compares a digest of the results with
the native build. CI runs each platform as its own job (see
[CONTRIBUTING.md](../CONTRIBUTING.md)).
