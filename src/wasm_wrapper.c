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

#include "cutils.h"
#include "mquickjs.h"

/* Required by stdlib */
static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
static JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { JS_GC(ctx); return JS_UNDEFINED; }
static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewInt64(ctx, 0); }
static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewInt64(ctx, 0); }
static JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_ThrowTypeError(ctx, "disabled"); }
static JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_ThrowTypeError(ctx, "disabled"); }
static JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_ThrowTypeError(ctx, "disabled"); }

#include "mqjs_stdlib.h"

static uint8_t *mem_buf = NULL;
static JSContext *ctx = NULL;
static char result_buf[65536];
static char error_buf[4096];

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
    JS_SetRandomSeed(ctx, 12345);
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
