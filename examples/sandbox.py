"""MQuickJS WASM sandbox wrapper for wasmtime."""

from wasmtime import Config, Engine, Linker, Module, Store, WasiConfig


class MQuickJSSandbox:
    """Wrapper for mquickjs WASM sandbox."""

    def __init__(self, wasm_path: str):
        config = Config()
        config.wasm_exceptions = True
        self.engine = Engine(config)
        self.store = Store(self.engine)

        module = Module.from_file(self.engine, wasm_path)

        linker = Linker(self.engine)
        linker.define_wasi()
        self.store.set_wasi(WasiConfig())
        self.instance = linker.instantiate(self.store, module)

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
