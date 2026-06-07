#include "telem_transport.h"

#ifdef CONFIG_ZMK_HUD_TELEMETRY_USB
void telem_usb_write(const uint8_t *buf, size_t len);
void telem_usb_init(void);
#endif
#ifdef CONFIG_ZMK_HUD_TELEMETRY_BLE
void telem_ble_write(const uint8_t *buf, size_t len);
void telem_ble_init(void);
#endif

void telem_transport_init(void)
{
#ifdef CONFIG_ZMK_HUD_TELEMETRY_USB
    telem_usb_init();
#endif
#ifdef CONFIG_ZMK_HUD_TELEMETRY_BLE
    telem_ble_init();
#endif
}

void telem_emit(const uint8_t *buf, size_t len)
{
#ifdef CONFIG_ZMK_HUD_TELEMETRY_USB
    telem_usb_write(buf, len);
#endif
#ifdef CONFIG_ZMK_HUD_TELEMETRY_BLE
    telem_ble_write(buf, len);
#endif
}
