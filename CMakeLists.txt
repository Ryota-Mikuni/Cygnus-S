zephyr_library()
zephyr_library_include_directories(config/include)

zephyr_library_sources(
  config/src/cygnus_gesture_state.c
  config/src/behavior_cygnus_gesture.c
  config/src/input_processor_cygnus_gesture.c
)
