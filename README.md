# mquickjs-wasm

Pre-built WebAssembly binaries of [Micro QuickJS][mquickjs] ("mquickjs") with support for the [finalized exception handling proposal][final-exception], intended for use in non-browser-based contexts.

## Download

Get the latest builds from [Releases](../../releases):

| File                       | WASI | Optimization | Size    | Use Case                      |
|----------------------------|------|--------------|---------|-------------------------------|
| `mquickjs.wasm`            | N    | `-O2`        | ~223 KB | Faster execution              |
| `mquickjs_small.wasm`      | N    | `-Oz`        | ~148 KB | Size-constrained environments |
| `mquickjs_wasi.wasm`       | Y    | `-O2`        | ~224 KB | Real time/random via WASI     |
| `mquickjs_wasi_small.wasm` | Y    | `-Oz`        | ~149 KB | WASI, size-optimized          |

The non-WASI builds return fixed values for `Date.now()` (`0`), `performance.now()` (`0`), and `Math.random()` (seeded). The WASI builds use WASI imports for real clock time and entropy:`Date.now()` returns actual time, `Math.random()` returns a random number.

Each release includes `SHA256SUMS`; verify with `sha256sum -c SHA256SUMS`.

## Build Locally

Local builds require [Nix](https://nixos.org/).

```bash
# Build all variants (default).
nix build  # Output: ./result/lib/mquickjs.wasm
           #         ./result/lib/mquickjs_small.wasm
           #         ./result/lib/mquickjs_wasi.wasm
           #         ./result/lib/mquickjs_wasi_small.wasm

# Build individual variants.
nix build .#mquickjs-wasm            # Standard (-O2)
nix build .#mquickjs-wasm-small      # Size-optimized (-Oz)
nix build .#mquickjs-wasm-wasi       # WASI (-O2)
nix build .#mquickjs-wasm-wasi-small # WASI size-optimized (-Oz)
```

## API

The WASM module exports:

| Function            | Signature       | Description                                                                         |
|---------------------|-----------------|-------------------------------------------------------------------------------------|
| `sandbox_init`      | `(i32) -> i32`  | Initialize with memory size in bytes (e.g., 1048576 for 1 MB), returns 1 on success |
| `sandbox_free`      | `() -> void`    | Free sandbox resources                                                              |
| `sandbox_eval`      | `(i32) -> i32`  | Evaluate JS code at pointer, returns result string pointer (0 on error)             |
| `sandbox_get_error` | `() -> i32`     | Get last error message pointer                                                      |
| `malloc`            | `(i32) -> i32`  | Allocate memory                                                                     |
| `free`              | `(i32) -> void` | Free memory                                                                         |

## Runtime Behaviour

The wrapper provides a minimal standalone runtime. API behavior differs between _non-WASI_ and _WASI_ build variants:

| API                 | Non-WASI            | WASI                       |
|---------------------|---------------------|----------------------------|
| `Date.now()`        | Returns `0`         | Real time (ms since epoch) |
| `performance.now()` | Returns `0`         | Real monotonic time (ms)   |
| `Math.random()`     | Seeded with `12345` | True randomness via WASI   |
| `print()`           | No-op               | No-op                      |
| `gc()`              | Works               | Works                      |
| `load()`            | Throws `TypeError`  | Throws `TypeError`         |
| `setTimeout()`      | Throws `TypeError`  | Throws `TypeError`         |
| `clearTimeout()`    | Throws `TypeError`  | Throws `TypeError`         |

## Examples

The [examples/](./examples/) directory contains Python demo scripts leveraging [wasmtime-py](https://github.com/bytecodealliance/wasmtime-py):

### Basic Demo

Demonstrates evaluation and exception handling. Run with [uv](https://docs.astral.sh/uv/):

```bash
uv run examples/demo.py ./result/lib/mquickjs.wasm
uv run examples/demo.py ./result/lib/mquickjs_small.wasm
```

If you have `nix` installed, but `uv` isn't in your `PATH`, use:

```bash
nix run nixpkgs#uv -- run examples/demo.py ./result/lib/mquickjs.wasm
```

Expected output:

```
Loading: ./result/lib/mquickjs.wasm

--- Adding numbers ---
40 + 2 = 42

--- Try/catch in JavaScript ---
Result: Caught: unexpected character

--- Uncaught exception ---
Caught error: Error: Something went wrong!

Done!
```

### WASI Demo

Demonstrates real system time and randomness. Requires a WASI build (`mquickjs_wasi.wasm` or `mquickjs_wasi_small.wasm`):

```bash
uv run examples/demo_wasi.py ./result/lib/mquickjs_wasi.wasm

uv run examples/demo_wasi.py ./result/lib/mquickjs_wasi_small.wasm
```

Expected output:

```
Loading: ./result/lib/mquickjs_wasi.wasm

--- System time via Date.now() ---
Timestamp: 1768089487125
Date/time: 2026-01-10 18:58:07.125000

--- Random numbers via Math.random() ---
  0.7234981623401856
  0.1892045678234901
  0.4567823401298345

Done!
```

(Timestamp and random values will vary on each run.)

See [examples/sandbox.py](./examples/sandbox.py) for a reusable `MQuickJSSandbox` wrapper class.

## Versioning

Micro QuickJS doesn't have formal releases or version numbers. This repo uses commit-based versioning:

```
vYYYY-MM-DD+<commit-hash>
```

### Automated Updates

This repo automatically tracks Micro QuickJS updates. The [CI script](./.github/workflows/ci.yml) periodically checks for new commits. When there is an update, the script updates [flake.nix](./flake.nix), builds the WASM artifacts, and creating a tagged release with all build variants.

## Technical Details

Micro QuickJS uses `setjmp`/`longjmp` for error handling. WebAssembly has two formats in WebAssembly for handling exceptions:

| Format                        | Emscripten Default | Wasmtime/Cranelift |
|-------------------------------|--------------------|--------------------|
| Legacy (Phase 1)              | Yes                | Not supported      |
| Finalized (Phase 4, `exnref`) | No                 | Supported          |

This repo builds Micro QuickJS using Emscripten with the finalized exception format (relevant flags include `-fwasm-exceptions -s SUPPORT_LONGJMP=wasm -s WASM_LEGACY_EXCEPTIONS=0`). The term `exnref` ("exception reference") refers to a value type in the [finalized exception handling proposal][final-exception] that allows exceptions to be passed as first-class values. This is part of the WebAssembly 3.0 specification, and is implemented by modern runtimes like Wasmtime's Cranelift backend.

## Acknowledgements

- Simon Willison, for his [exploratory work on sandboxing Micro QuickJS](https://github.com/simonw/research/tree/main/mquickjs-sandbox).
- Fabrice Bellard and Charlie Gordon, for creating [MQuickJS][mquickjs].

## License

The build scripts and supporting code in this repository are released under the terms of the [MIT License](./LICENSE).



[final-exception]: https://github.com/WebAssembly/exception-handling/blob/main/proposals/exception-handling/Exceptions.md
[mquickjs]: https://github.com/bellard/mquickjs
