# zmk-hud-telemetry

ZMK module that streams keyboard state (layer, key presses, battery, endpoint)
from the central half to a host HUD app. See the protocol in the keyboard-hud
repo: `PROTOCOL.md`. Enable with `CONFIG_ZMK_HUD_TELEMETRY=y` on the central build.

Transports (both behind one `telem_emit` interface):
- `CONFIG_ZMK_HUD_TELEMETRY_BLE` — custom GATT notify characteristic (primary, wireless)
- `CONFIG_ZMK_HUD_TELEMETRY_USB` — USB CDC-ACM serial (dev/debug + fallback)

## Host-side formatter test
The NDJSON line formatter (`src/telem_format.c`) is Zephyr-free C with a host unit test:

```
cd tests/host && make test
```
