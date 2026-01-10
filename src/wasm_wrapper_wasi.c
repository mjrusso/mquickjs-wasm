#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT __attribute__((visibility("default")))
#endif

/* WASI imports for clock and random */
#include <time.h>

/* For WASI random_get */
#ifdef __wasi__
#include <wasi/api.h>
#else
/* Fallback for non-WASI builds */
uint16_t __wasi_random_get(uint8_t *buf, size_t buf_len) {
    for (size_t i = 0; i < buf_len; i++) {
        buf[i] = (uint8_t)rand();
    }
    return 0;
}
#endif

#include "cutils.h"
#include "mquickjs.h"

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JS_GC(ctx);
    return JS_UNDEFINED;
}

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        int64_t ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        return JS_NewInt64(ctx, ms);
    }
    return JS_NewInt64(ctx, 0);
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        /* Return as float for sub-ms precision */
        double ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
        return JS_NewFloat64(ctx, ms);
    }
    return JS_NewInt64(ctx, 0);
}

static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_ThrowTypeError(ctx, "load() not available");
}

static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_ThrowTypeError(ctx, "setTimeout() not available - no async support");
}

static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_ThrowTypeError(ctx, "clearTimeout() not available - no async support");
}

#include "mqjs_stdlib.h"

static uint8_t *mem_buf = NULL;
static JSContext *ctx = NULL;
static char result_buf[65536];
static char error_buf[4096];

static uint64_t get_random_seed(void) {
    uint64_t seed;
    if (__wasi_random_get((uint8_t *)&seed, sizeof(seed)) == 0) {
        return seed;
    }
    return 12345; /* Fallback */
}

EXPORT
int sandbox_init(int mem_size) {
    if (mem_buf) free(mem_buf);
    mem_buf = malloc(mem_size);
    if (!mem_buf) return 0;

    ctx = JS_NewContext(mem_buf, mem_size, &js_stdlib);
    if (!ctx) {
        free(mem_buf);
        mem_buf = NULL;
        return 0;
    }

    /* Seed random from WASI entropy source */
    JS_SetRandomSeed(ctx, get_random_seed());
    return 1;
}

EXPORT
void sandbox_free() {
    if (ctx) { JS_FreeContext(ctx); ctx = NULL; }
    if (mem_buf) { free(mem_buf); mem_buf = NULL; }
}

EXPORT
const char* sandbox_eval(const char *code) {
    if (!ctx) {
        strcpy(error_buf, "Not initialized");
        return NULL;
    }

    error_buf[0] = 0;
    result_buf[0] = 0;

    JSValue val = JS_Eval(ctx, code, strlen(code), "<sandbox>", JS_EVAL_RETVAL);

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(ctx);
        if (JS_IsString(ctx, exc)) {
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, exc, &buf);
            if (str) {
                strncpy(error_buf, str, sizeof(error_buf) - 1);
                error_buf[sizeof(error_buf) - 1] = 0;
            } else {
                strcpy(error_buf, "Unknown error");
            }
        } else {
            JSValue str_val = JS_ToString(ctx, exc);
            if (JS_IsString(ctx, str_val)) {
                JSCStringBuf buf;
                const char *str = JS_ToCString(ctx, str_val, &buf);
                if (str) {
                    strncpy(error_buf, str, sizeof(error_buf) - 1);
                    error_buf[sizeof(error_buf) - 1] = 0;
                } else {
                    strcpy(error_buf, "Unknown error");
                }
            } else {
                strcpy(error_buf, "Unknown error");
            }
        }
        return NULL;
    }

    if (JS_IsUndefined(val)) {
        strcpy(result_buf, "undefined");
    } else if (JS_IsNull(val)) {
        strcpy(result_buf, "null");
    } else if (JS_IsBool(val)) {
        strcpy(result_buf, val == JS_TRUE ? "true" : "false");
    } else if (JS_IsInt(val)) {
        snprintf(result_buf, sizeof(result_buf), "%d", JS_VALUE_GET_INT(val));
    } else if (JS_IsNumber(ctx, val)) {
        double d;
        JS_ToNumber(ctx, &d, val);
        snprintf(result_buf, sizeof(result_buf), "%.17g", d);
    } else if (JS_IsString(ctx, val)) {
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, val, &buf);
        if (str) {
            strncpy(result_buf, str, sizeof(result_buf) - 1);
            result_buf[sizeof(result_buf) - 1] = 0;
        }
    } else {
        JSValue str_val = JS_ToString(ctx, val);
        if (JS_IsString(ctx, str_val)) {
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, str_val, &buf);
            if (str) {
                strncpy(result_buf, str, sizeof(result_buf) - 1);
                result_buf[sizeof(result_buf) - 1] = 0;
            } else {
                strcpy(result_buf, "[object]");
            }
        } else {
            strcpy(result_buf, "[object]");
        }
    }

    return result_buf;
}

EXPORT
const char* sandbox_get_error() {
    return error_buf;
}
