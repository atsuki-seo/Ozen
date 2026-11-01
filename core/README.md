# The Ozen compiler core

`core/` is the compiler in C23 with no I/O of its own: parsing, imports, compile-time
evaluation, type checking (dimensions, states, linear use of quantities and resources),
the IR, and the scheduler. The [`ozen` command](../cli/README.md) reads the files and
calls it, and the [platform bindings](../bindings/README.md) call it on other platforms.

## Public API

The public API is [`core/include/ozen.h`](include/ozen.h) (`recipe_build` returns the
result as JSON). The JSON outputs follow the schemas in [`schemas/`](../schemas/)
(draft 2020-12), and `--format sarif` follows SARIF 2.1.0.

## Building

`scripts/build-core.sh` builds the core and the `ozen` command. See
[CONTRIBUTING.md](../CONTRIBUTING.md) for the four configurations and the golden tests.
