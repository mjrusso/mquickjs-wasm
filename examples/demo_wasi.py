#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["wasmtime>=40.0.0"]
# ///
"""Demo of mquickjs WASM sandbox with WASI features (real time and randomness)."""

import sys

from datetime import datetime

from sandbox import MQuickJSSandbox


def main():
    if len(sys.argv) < 2:
        print("Usage: demo_wasi.py <path-to-wasi-wasm>")
        sys.exit(1)

    wasm_path = sys.argv[1]
    print(f"Loading: {wasm_path}")

    sandbox = MQuickJSSandbox(wasm_path)
    sandbox.init()

    print("\n--- System time via Date.now() ---")
    success, result = sandbox.eval("Date.now()")
    if success:
        ts = int(result)
        print(f"Timestamp: {ts}")
        dt = datetime.fromtimestamp(ts / 1000)
        print(f"Date/time: {dt}")

    print("\n--- Random numbers via Math.random() ---")
    for i in range(3):
        success, result = sandbox.eval("Math.random()")
        print(f"  {result}")

    sandbox.free()
    print("\nDone!")


if __name__ == "__main__":
    main()
