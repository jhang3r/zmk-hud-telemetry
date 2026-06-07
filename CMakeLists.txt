if(CONFIG_ZMK_HUD_TELEMETRY)
  zephyr_library_named(zmk_hud_telemetry)
  zephyr_include_directories(include)
  zephyr_library_sources(src/telem_format.c)
  # src/telem.c, src/telem_emit.c, src/telem_usb.c, src/telem_ble.c are added
  # in later tasks as those sources are created.
endif()
