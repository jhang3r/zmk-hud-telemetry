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

#endif
