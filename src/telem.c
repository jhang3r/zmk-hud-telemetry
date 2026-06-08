#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/battery.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>

#include "zmk_hud/telem_format.h"
#include "telem_transport.h"

#define LINE_MAX  CONFIG_ZMK_HUD_TELEMETRY_LINE_MAX
#define KEY_COUNT CONFIG_ZMK_HUD_TELEMETRY_KEY_COUNT

/* code events carry no position; send a sentinel the app ignores for legends */
#define NO_POSITION 65535

void telem_send_keymap(void);          /* defined below; called from snapshot */
static void keymap_poll_start(void);   /* defined below; started from init */

/* ---- helpers ---------------------------------------------------------- */

/* Fill `out` with the active layer indices (lowest->highest). Returns count. */
static size_t active_layers(uint8_t *out, size_t cap)
{
    zmk_keymap_layers_state_t state = zmk_keymap_layer_state();
    size_t n = 0;
    for (uint8_t i = 0; i < ZMK_KEYMAP_LAYERS_LEN && n < cap; i++) {
        if (state & (((zmk_keymap_layers_state_t)1) << i)) {
            out[n++] = i;
        }
    }
    if (n == 0 && cap > 0) { out[0] = 0; n = 1; } /* default layer always on */
    return n;
}

static const char *ep_kind_str(void)
{
    struct zmk_endpoint_instance ep = zmk_endpoints_selected();
    return ep.transport == ZMK_TRANSPORT_USB ? "usb" : "ble";
}

static int ep_profile_idx(void)
{
    struct zmk_endpoint_instance ep = zmk_endpoints_selected();
    return ep.transport == ZMK_TRANSPORT_BLE ? ep.ble.profile_index : -1;
}

/* v0.3.0 has no ep_connected(): USB is "connected" when selected;
 * BLE is connected when the active profile has a live connection. */
static bool ep_connected(void)
{
    struct zmk_endpoint_instance ep = zmk_endpoints_selected();
    if (ep.transport == ZMK_TRANSPORT_USB) return true;
    return zmk_ble_active_profile_is_connected();
}

/* peripheral battery cached from events (no direct query API on central) */
static int s_batt_peripheral = -1;

/* ---- snapshot --------------------------------------------------------- */

void telem_send_snapshot(void)
{
    char line[LINE_MAX];
    int n;

    n = telem_fmt_hello(line, sizeof line, "handwired_split");
    if (n > 0) telem_emit((uint8_t *)line, (size_t)n);

    uint8_t layers[ZMK_KEYMAP_LAYERS_LEN];
    size_t nl = active_layers(layers, sizeof layers);
    n = telem_fmt_snapshot(line, sizeof line, layers, nl,
                           (int)zmk_battery_state_of_charge(),
                           s_batt_peripheral,
                           ep_kind_str(), ep_profile_idx(),
                           ep_connected());
    if (n > 0) telem_emit((uint8_t *)line, (size_t)n);

    telem_send_keymap(); /* stream the live keymap right after the snapshot */
}

/* ---- keymap dump ------------------------------------------------------ *
 * The app derives key legends from this stream (not from the source .keymap
 * file), so it reflects runtime edits made via ZMK Studio. The dump is paced
 * across work-queue ticks because BLE notify buffers are small and a tight
 * loop would overflow them and drop most lines.
 *
 * Layer numbering matches the `lyr`/`snapshot` messages: the layer-state
 * bitmask is indexed by layer id, so we iterate ids 0..LAYERS_LEN-1 and send
 * the id as "l". (For a keymap whose layers haven't been reordered/added via
 * Studio, id == index, which is what the app's keymap order expects.) */

#define KEYMAP_DUMP_BATCH 4   /* lines emitted per pump tick */
#define KEYMAP_DUMP_GAP_MS 20 /* delay between pump ticks (lets BLE drain) */

static struct {
    bool active;
    int phase;       /* 0=begin, 1=klyr, 2=bind, 3=end, 4=done */
    uint8_t layer;   /* current layer id */
    uint16_t pos;    /* current key position (bind phase) */
} s_dump;

/* Emit one line, advance the cursor. Returns true if more remain. */
static bool keymap_dump_step(void)
{
    char line[LINE_MAX];
    int n;

    switch (s_dump.phase) {
    case 0:
        n = telem_fmt_kmap_begin(line, sizeof line, ZMK_KEYMAP_LAYERS_LEN, KEY_COUNT);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
        s_dump.phase = 1;
        s_dump.layer = 0;
        return true;
    case 1: {
        const char *name = zmk_keymap_layer_name((zmk_keymap_layer_id_t)s_dump.layer);
        n = telem_fmt_klyr(line, sizeof line, s_dump.layer, name);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
        if (++s_dump.layer >= ZMK_KEYMAP_LAYERS_LEN) {
            s_dump.phase = 2;
            s_dump.layer = 0;
            s_dump.pos = 0;
        }
        return true;
    }
    case 2: {
        const struct zmk_behavior_binding *b =
            zmk_keymap_get_layer_binding_at_idx((zmk_keymap_layer_id_t)s_dump.layer,
                                                (uint8_t)s_dump.pos);
        n = telem_fmt_bind(line, sizeof line, s_dump.layer, s_dump.pos,
                           b ? b->behavior_dev : NULL,
                           b ? b->param1 : 0, b ? b->param2 : 0);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
        if (++s_dump.pos >= KEY_COUNT) {
            s_dump.pos = 0;
            if (++s_dump.layer >= ZMK_KEYMAP_LAYERS_LEN) s_dump.phase = 3;
        }
        return true;
    }
    case 3:
        n = telem_fmt_kmap_end(line, sizeof line);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
        s_dump.phase = 4;
        return false;
    default:
        return false;
    }
}

static void keymap_pump_handler(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(keymap_pump, keymap_pump_handler);

static void keymap_pump_handler(struct k_work *w)
{
    ARG_UNUSED(w);
    for (int i = 0; i < KEYMAP_DUMP_BATCH; i++) {
        if (!keymap_dump_step()) {
            s_dump.active = false;
            return;
        }
    }
    k_work_reschedule(&keymap_pump, K_MSEC(KEYMAP_DUMP_GAP_MS));
}

void telem_send_keymap(void)
{
    /* (re)start from the top; safe to call while a dump is in flight */
    s_dump.active = true;
    s_dump.phase = 0;
    s_dump.layer = 0;
    s_dump.pos = 0;
    k_work_reschedule(&keymap_pump, K_NO_WAIT);
}

/* ---- keymap change detection (poll, since ZMK raises no binding event) -- */

#if CONFIG_ZMK_HUD_TELEMETRY_KEYMAP_POLL_MS > 0

static uint32_t keymap_checksum(void)
{
    uint32_t h = 2166136261u; /* FNV-1a */
    for (uint8_t l = 0; l < ZMK_KEYMAP_LAYERS_LEN; l++) {
        for (uint16_t p = 0; p < KEY_COUNT; p++) {
            const struct zmk_behavior_binding *b =
                zmk_keymap_get_layer_binding_at_idx((zmk_keymap_layer_id_t)l, (uint8_t)p);
            uint32_t v1 = b ? b->param1 : 0;
            uint32_t v2 = b ? b->param2 : 0;
            uint32_t vd = b ? (uint32_t)(uintptr_t)b->behavior_dev : 0;
            h = (h ^ v1) * 16777619u;
            h = (h ^ v2) * 16777619u;
            h = (h ^ vd) * 16777619u; /* behavior_dev ptr is stable per behavior */
        }
    }
    return h;
}

static void keymap_poll_handler(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(keymap_poll, keymap_poll_handler);

static void keymap_poll_handler(struct k_work *w)
{
    ARG_UNUSED(w);
    static bool primed;
    static uint32_t last_sum;

    uint32_t sum = keymap_checksum();
    if (!primed) {
        primed = true;          /* establish baseline; connect already dumped */
        last_sum = sum;
    } else if (sum != last_sum) {
        last_sum = sum;
        if (!s_dump.active) telem_send_keymap();
    }
    k_work_reschedule(&keymap_poll, K_MSEC(CONFIG_ZMK_HUD_TELEMETRY_KEYMAP_POLL_MS));
}

static void keymap_poll_start(void)
{
    k_work_reschedule(&keymap_poll, K_MSEC(CONFIG_ZMK_HUD_TELEMETRY_KEYMAP_POLL_MS));
}

#else
static void keymap_poll_start(void) {}
#endif

/* ---- listeners -------------------------------------------------------- */

static int on_layer(const zmk_event_t *eh)
{
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev) {
        char line[LINE_MAX];
        uint8_t layers[ZMK_KEYMAP_LAYERS_LEN];
        size_t nl = active_layers(layers, sizeof layers);
        int n = telem_fmt_layers(line, sizeof line, layers, nl);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int on_position(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev) {
        char line[LINE_MAX];
        int n = telem_fmt_pos(line, sizeof line, ev->position, ev->state);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int on_keycode(const zmk_event_t *eh)
{
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev) {
        char line[LINE_MAX];
        int n = telem_fmt_code(line, sizeof line, NO_POSITION,
                               ev->usage_page, ev->keycode, ev->state);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int on_battery(const zmk_event_t *eh)
{
    char line[LINE_MAX];
    const struct zmk_battery_state_changed *c = as_zmk_battery_state_changed(eh);
    if (c) {
        int n = telem_fmt_batt(line, sizeof line, 'c', c->state_of_charge);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    const struct zmk_peripheral_battery_state_changed *p =
        as_zmk_peripheral_battery_state_changed(eh);
    if (p) {
        s_batt_peripheral = p->state_of_charge;
        int n = telem_fmt_batt(line, sizeof line, 'p', p->state_of_charge);
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int on_endpoint(const zmk_event_t *eh)
{
    if (as_zmk_endpoint_changed(eh) || as_zmk_ble_active_profile_changed(eh)) {
        char line[LINE_MAX];
        int n = telem_fmt_ep(line, sizeof line, ep_kind_str(),
                             ep_profile_idx(), ep_connected());
        if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(hud_layer, on_layer);
ZMK_SUBSCRIPTION(hud_layer, zmk_layer_state_changed);

ZMK_LISTENER(hud_pos, on_position);
ZMK_SUBSCRIPTION(hud_pos, zmk_position_state_changed);

ZMK_LISTENER(hud_code, on_keycode);
ZMK_SUBSCRIPTION(hud_code, zmk_keycode_state_changed);

ZMK_LISTENER(hud_batt, on_battery);
ZMK_SUBSCRIPTION(hud_batt, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(hud_batt, zmk_peripheral_battery_state_changed);

ZMK_LISTENER(hud_ep, on_endpoint);
ZMK_SUBSCRIPTION(hud_ep, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(hud_ep, zmk_ble_active_profile_changed);

static int hud_init(void)
{
    telem_transport_init();
    keymap_poll_start();
    return 0;
}
SYS_INIT(hud_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
