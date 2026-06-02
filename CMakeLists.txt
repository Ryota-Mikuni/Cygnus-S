zephyr_library()

zephyr_include_directories(config/include include)

# The keymap references &cy_gesture even in non-pointing builds such as
# settings_reset, so the behavior and shared state must always be available.
zephyr_library_sources(config/src/cygnus_gesture_state.c)
zephyr_library_sources(config/src/behavior_cygnus_gesture.c)
zephyr_library_sources_ifdef(CONFIG_CYGNUS_GESTURE config/src/input_processor_cygnus_gesture.c)
