zephyr_library()
zephyr_include_directories(config/include)

# The keymap may be parsed even in settings_reset builds, so the custom
# behavior and shared state must always be available. The input processor is
# only needed when mouse/pointing support is enabled.
zephyr_library_sources(config/src/cygnus_gesture_state.c)
zephyr_library_sources(config/src/behavior_cygnus_gesture.c)
zephyr_library_sources_ifdef(CONFIG_CYGNUS_GESTURE config/src/input_processor_cygnus_gesture.c)
