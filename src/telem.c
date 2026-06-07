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
#include <zmk/battery.h>
#include <zmk/endpoints.h>

#include "zmk_hud/telem_format.h"
#include "telem_transport.h"

#define LINE_MAX CONFIG_ZMK_HUD_TELEMETRY_LINE_MAX

/* code events carry no position; send a sentinel the app ignores for legends */
#define NO_POSITION 65535

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
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    return ep.transport == ZMK_TRANSPORT_USB ? "usb" : "ble";
}

static int ep_profile_idx(void)
{
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    return ep.transport == ZMK_TRANSPORT_BLE ? ep.ble.profile_index : -1;
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
                           zmk_endpoint_is_connected());
    if (n > 0) telem_emit((uint8_t *)line, (size_t)n);
}

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
                             ep_profile_idx(), zmk_endpoint_is_connected());
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
    return 0;
}
SYS_INIT(hud_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
