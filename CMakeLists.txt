if(CONFIG_ZMK_HUD_TELEMETRY)
  zephyr_library_named(zmk_hud_telemetry)
  zephyr_include_directories(include)
  # ZMK's internal headers (event bus, events, keymap, endpoints, ble, battery)
  # live in zmk/app/include, which is NOT on the external-module include path by
  # default. zmk is a sibling of zephyr in the west workspace.
  zephyr_include_directories(${ZEPHYR_BASE}/../zmk/app/include)
  zephyr_library_sources(src/telem_format.c)
  zephyr_library_sources(src/telem.c)
  zephyr_library_sources(src/telem_emit.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_HUD_TELEMETRY_USB src/telem_usb.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_HUD_TELEMETRY_BLE src/telem_ble.c)
endif()
