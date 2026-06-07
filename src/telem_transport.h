#ifndef ZMK_HUD_TELEM_TRANSPORT_H
#define ZMK_HUD_TELEM_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

/* Called once at module init. */
void telem_transport_init(void);

/* Emit one already-formatted line (includes trailing '\n') to all active
 * transports. Safe to call from ZMK event listener context. */
void telem_emit(const uint8_t *buf, size_t len);

/* Implemented in telem.c: builds and emits the full snapshot. Called by a
 * transport when a host connects/subscribes. */
void telem_send_snapshot(void);

#endif
