#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["wasmtime>=40.0.0"]
# ///
"""Basic demo of mquickjs WASM sandbox."""

import sys

from sandbox import MQuickJSSandbox


def main():
    if len(sys.argv) < 2:
        print("Usage: demo.py <path-to-wasm>")
        sys.exit(1)

    wasm_path = sys.argv[1]
    print(f"Loading: {wasm_path}")

    sandbox = MQuickJSSandbox(wasm_path)
    sandbox.init()

    print("\n--- Adding numbers ---")
    success, result = sandbox.eval("40 + 2")
    print(f"40 + 2 = {result}")

    print("\n--- Try/catch in JavaScript ---")
    success, result = sandbox.eval("""
        try {
            JSON.parse('not valid json');
        } catch (e) {
            'Caught: ' + e.message;
        }
    """)
    print(f"Result: {result}")

    print("\n--- Uncaught exception ---")
    success, result = sandbox.eval("throw new Error('Something went wrong!')")
    if not success:
        print(f"Caught error: {result}")

    sandbox.free()
    print("\nDone!")


if __name__ == "__main__":
    main()
