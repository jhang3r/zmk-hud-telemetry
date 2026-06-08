#include "zmk_hud/telem_format.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* snprintf into out; return bytes written (excl NUL) or -1 on truncation. */
static int emit(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/* writes "[0,1]" into out; returns length or -1 */
static int emit_layers_array(char *out, size_t cap, const uint8_t *layers, size_t n)
{
    size_t pos = 0;
    if (pos + 1 >= cap) return -1;
    out[pos++] = '[';
    for (size_t i = 0; i < n; i++) {
        int w = snprintf(out + pos, cap - pos, "%s%u", i ? "," : "", layers[i]);
        if (w < 0 || (size_t)w >= cap - pos) return -1;
        pos += (size_t)w;
    }
    if (pos + 1 >= cap) return -1;
    out[pos++] = ']';
    out[pos] = '\0';
    return (int)pos;
}

int telem_fmt_hello(char *out, size_t cap, const char *kb)
{
    return emit(out, cap, "{\"t\":\"hello\",\"pv\":1,\"kb\":\"%s\"}\n", kb);
}

int telem_fmt_layers(char *out, size_t cap, const uint8_t *layers, size_t n)
{
    char arr[64];
    if (emit_layers_array(arr, sizeof arr, layers, n) < 0) return -1;
    return emit(out, cap, "{\"t\":\"lyr\",\"lyr\":%s}\n", arr);
}

int telem_fmt_snapshot(char *out, size_t cap,
                       const uint8_t *layers, size_t n_layers,
                       int batt_central, int batt_peripheral,
                       const char *ep_kind, int ep_profile, bool ep_on)
{
    char arr[64];
    if (emit_layers_array(arr, sizeof arr, layers, n_layers) < 0) return -1;
    return emit(out, cap,
        "{\"t\":\"snapshot\",\"lyr\":%s,\"bat\":{\"c\":%d,\"p\":%d},"
        "\"ep\":{\"k\":\"%s\",\"pf\":%d,\"on\":%s}}\n",
        arr, batt_central, batt_peripheral, ep_kind, ep_profile,
        ep_on ? "true" : "false");
}

int telem_fmt_pos(char *out, size_t cap, uint32_t position, bool down)
{
    return emit(out, cap, "{\"t\":\"pos\",\"p\":%u,\"d\":%s}\n",
                position, down ? "true" : "false");
}

int telem_fmt_code(char *out, size_t cap, uint32_t position,
                   uint16_t usage_page, uint32_t keycode, bool down)
{
    return emit(out, cap, "{\"t\":\"code\",\"p\":%u,\"pg\":%u,\"c\":%u,\"d\":%s}\n",
                position, usage_page, keycode, down ? "true" : "false");
}

int telem_fmt_batt(char *out, size_t cap, char src, uint8_t pct)
{
    return emit(out, cap, "{\"t\":\"bat\",\"s\":\"%c\",\"v\":%u}\n", src, pct);
}

int telem_fmt_ep(char *out, size_t cap, const char *kind, int profile, bool on)
{
    return emit(out, cap, "{\"t\":\"ep\",\"k\":\"%s\",\"pf\":%d,\"on\":%s}\n",
                kind, profile, on ? "true" : "false");
}

int telem_fmt_kmap_begin(char *out, size_t cap, int layer_count, int key_count)
{
    return emit(out, cap, "{\"t\":\"kmap\",\"lc\":%d,\"kc\":%d}\n",
                layer_count, key_count);
}

int telem_fmt_klyr(char *out, size_t cap, int layer, const char *name)
{
    return emit(out, cap, "{\"t\":\"klyr\",\"l\":%d,\"n\":\"%s\"}\n",
                layer, name ? name : "");
}

int telem_fmt_bind(char *out, size_t cap, int layer, int pos,
                   const char *behavior, uint32_t p1, uint32_t p2)
{
    return emit(out, cap,
        "{\"t\":\"bind\",\"l\":%d,\"p\":%d,\"b\":\"%s\",\"p1\":%u,\"p2\":%u}\n",
        layer, pos, behavior ? behavior : "", p1, p2);
}

int telem_fmt_kmap_end(char *out, size_t cap)
{
    return emit(out, cap, "{\"t\":\"kmend\"}\n");
}
