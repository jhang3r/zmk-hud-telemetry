#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "telem_transport.h"

/* The telemetry CDC-ACM node is added by a devicetree overlay on the central
 * (left) shield build. If it's missing, fail loudly with a clear message. */
#if !DT_NODE_EXISTS(DT_NODELABEL(hud_telemetry_cdc))
#error "hud_telemetry_cdc devicetree node missing (add the CDC-ACM overlay to the central shield)"
#endif

static const struct device *const telem_uart =
    DEVICE_DT_GET(DT_NODELABEL(hud_telemetry_cdc));

static bool s_open; /* host has the port open (DTR asserted) */

void telem_usb_init(void)
{
    /* ZMK already calls usb_enable(); the CDC-ACM device is statically ready. */
}

void telem_usb_write(const uint8_t *buf, size_t len)
{
    if (!device_is_ready(telem_uart)) return;

    uint32_t dtr = 0;
    uart_line_ctrl_get(telem_uart, UART_LINE_CTRL_DTR, &dtr);
    if (!dtr) { s_open = false; return; }

    /* On the open transition, push a full snapshot first. Setting s_open BEFORE
     * the snapshot call prevents re-entrant recursion (snapshot -> telem_emit ->
     * telem_usb_write sees s_open == true and skips this branch). */
    if (!s_open) {
        s_open = true;
        telem_send_snapshot();
    }

    for (size_t i = 0; i < len; i++) {
        uart_poll_out(telem_uart, buf[i]);
    }
}
