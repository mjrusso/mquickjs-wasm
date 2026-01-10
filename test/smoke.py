#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["wasmtime"]
# ///
"""Smoke test for mquickjs WASM builds using wasmtime."""

import argparse
import sys

from wasmtime import Config, Engine, Instance, Linker, Module, Store, WasiConfig


class MQuickJSSandbox:
    """Wrapper for mquickjs WASM sandbox."""

    def __init__(self, wasm_path: str):
        config = Config()
        config.wasm_exceptions = True
        self.engine = Engine(config)
        self.store = Store(self.engine)

        module = Module.from_file(self.engine, wasm_path)

        needs_wasi = any(imp.module.startswith("wasi") for imp in module.imports)

        if needs_wasi:
            linker = Linker(self.engine)
            linker.define_wasi()
            wasi_config = WasiConfig()
            self.store.set_wasi(wasi_config)
            self.instance = linker.instantiate(self.store, module)
        else:
            self.instance = Instance(self.store, module, [])

        exports = self.instance.exports(self.store)
        self._sandbox_init = exports["sandbox_init"]
        self._sandbox_eval = exports["sandbox_eval"]
        self._sandbox_free = exports["sandbox_free"]
        self._sandbox_get_error = exports["sandbox_get_error"]
        self._malloc = exports["malloc"]
        self._free = exports["free"]
        self._memory = exports["memory"]

    def init(self, mem_size: int = 1024 * 1024) -> bool:
        return self._sandbox_init(self.store, mem_size) == 1

    def eval(self, js_code: str) -> tuple[bool, str]:
        """Evaluate JS code. Returns (success, result_or_error)."""
        code_bytes = js_code.encode("utf-8") + b"\0"
        code_ptr = self._malloc(self.store, len(code_bytes))

        mem_data = self._memory.data_ptr(self.store)
        for i, b in enumerate(code_bytes):
            mem_data[code_ptr + i] = b

        result_ptr = self._sandbox_eval(self.store, code_ptr)

        if result_ptr == 0:
            error_ptr = self._sandbox_get_error(self.store)
            result = self._read_string(mem_data, error_ptr)
            success = False
        else:
            result = self._read_string(mem_data, result_ptr)
            success = True

        self._free(self.store, code_ptr)
        return success, result

    def _read_string(self, mem_data, ptr: int) -> str:
        result_bytes = bytearray()
        i = 0
        while mem_data[ptr + i] != 0:
            result_bytes.append(mem_data[ptr + i])
            i += 1
        return result_bytes.decode("utf-8")

    def free(self):
        self._sandbox_free(self.store)


def test_wasm(wasm_path: str, wasi: bool) -> bool:
    """Test a mquickjs WASM file."""
    print(f"Testing {wasm_path} (wasi={wasi})")

    try:
        sandbox = MQuickJSSandbox(wasm_path)
    except Exception as e:
        print(f"ERROR: Failed to load module: {e}")
        return False

    if not sandbox.init():
        print("ERROR: sandbox_init failed")
        return False
    print("OK: sandbox_init succeeded")

    all_passed = True

    # Common test cases
    test_cases = [
        ("1 + 2", "3"),
        ("'hello' + ' ' + 'world'", "hello world"),
        ("Math.sqrt(16)", "4"),
        ("JSON.stringify({a: 1})", '{"a":1}'),
        ("var x = 10; x * 2", "20"),
    ]

    for js_code, expected in test_cases:
        success, result = sandbox.eval(js_code)
        if not success:
            print(f"FAIL: '{js_code}' -> error: {result}")
            all_passed = False
        elif result == expected:
            print(f"OK: '{js_code}' -> '{result}'")
        else:
            print(f"FAIL: '{js_code}' -> '{result}' (expected '{expected}')")
            all_passed = False

    if wasi:
        print("\n--- WASI-specific tests ---")

        # Date.now() should return non-zero (real timestamp)
        success, result = sandbox.eval("Date.now()")
        if success and result != "0":
            try:
                ts = int(result)
                if ts > 1700000000000:  # After 2023
                    print(f"OK: Date.now() returned real timestamp: {result}")
                else:
                    print(f"WARN: Date.now() returned low value: {result}")
            except ValueError:
                print(f"FAIL: Date.now() returned non-integer: {result}")
                all_passed = False
        else:
            print(f"FAIL: Date.now() should return real timestamp, got: {result}")
            all_passed = False

        # Math.random() should return different values
        success1, rand1 = sandbox.eval("Math.random()")
        success2, rand2 = sandbox.eval("Math.random()")
        if success1 and success2 and rand1 != rand2:
            print(f"OK: Math.random() returns varying values: {rand1}, {rand2}")
        elif success1 and success2:
            print(f"WARN: Math.random() returned same value twice: {rand1} (may be coincidence)")
        else:
            print(f"FAIL: Math.random() failed")
            all_passed = False

    else:
        print("\n--- Deterministic behavior tests ---")

        # Date.now() should return 0
        success, result = sandbox.eval("Date.now()")
        if success and result == "0":
            print("OK: Date.now() returns 0 (deterministic)")
        else:
            print(f"FAIL: Date.now() should return 0 for deterministic build, got: {result}")
            all_passed = False

        # Math.random() should be seeded (same sequence)
        sandbox.free()
        sandbox = MQuickJSSandbox(wasm_path)
        sandbox.init()
        success1, rand1 = sandbox.eval("Math.random()")

        sandbox.free()
        sandbox = MQuickJSSandbox(wasm_path)
        sandbox.init()
        success2, rand2 = sandbox.eval("Math.random()")

        if success1 and success2 and rand1 == rand2:
            print(f"OK: Math.random() is deterministic: {rand1}")
        else:
            print(f"FAIL: Math.random() should be deterministic, got: {rand1} vs {rand2}")
            all_passed = False

    sandbox.free()
    print("OK: sandbox_free succeeded")

    print()
    if all_passed:
        print("PASSED")
    else:
        print("FAILED")

    return all_passed


def main():
    parser = argparse.ArgumentParser(description="Smoke test for mquickjs WASM builds")
    parser.add_argument("wasm_path", help="Path to the WASM file to test")
    parser.add_argument("--wasi", action="store_true", help="Run WASI-specific tests (real time/random)")
    args = parser.parse_args()

    if test_wasm(args.wasm_path, args.wasi):
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
