#ifndef ZMK_HUD_TELEM_FORMAT_H
#define ZMK_HUD_TELEM_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* All functions write a single '\n'-terminated JSON line into `out` (capacity
 * `cap`) and return the number of bytes written excluding the NUL, or -1 if the
 * line did not fit. `out` is always NUL-terminated when return >= 0. */

int telem_fmt_hello(char *out, size_t cap, const char *kb);

/* layers: array of active layer indices, lowest->highest; n = count */
int telem_fmt_layers(char *out, size_t cap, const uint8_t *layers, size_t n);

int telem_fmt_snapshot(char *out, size_t cap,
                       const uint8_t *layers, size_t n_layers,
                       int batt_central, int batt_peripheral,
                       const char *ep_kind, int ep_profile, bool ep_on);

int telem_fmt_pos(char *out, size_t cap, uint32_t position, bool down);

int telem_fmt_code(char *out, size_t cap, uint32_t position,
                   uint16_t usage_page, uint32_t keycode, bool down);

/* src: 'c' central or 'p' peripheral */
int telem_fmt_batt(char *out, size_t cap, char src, uint8_t pct);

int telem_fmt_ep(char *out, size_t cap, const char *kind, int profile, bool on);

/* ---- keymap dump ------------------------------------------------------ *
 * A full keymap is streamed as: one kmap_begin, then for each layer a klyr
 * (layer name) and one bind per key position, then one kmap_end. The host
 * buffers between begin and end, then swaps in the new keymap atomically. */

int telem_fmt_kmap_begin(char *out, size_t cap, int layer_count, int key_count);

int telem_fmt_klyr(char *out, size_t cap, int layer, const char *name);

/* behavior: the binding's behavior_dev name; NULL renders as "". p1/p2 are the
 * raw binding params (for &kp, p1 packs the HID usage as (page<<16)|id). */
int telem_fmt_bind(char *out, size_t cap, int layer, int pos,
                   const char *behavior, uint32_t p1, uint32_t p2);

int telem_fmt_kmap_end(char *out, size_t cap);

#endif
