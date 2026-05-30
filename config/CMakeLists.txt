zephyr_include_directories(include)

if(CONFIG_CYGNUS_GESTURE)
  target_sources(app PRIVATE
    src/cygnus_gesture_state.c
    src/behavior_cygnus_gesture.c
    src/input_processor_cygnus_gesture.c
  )
endif()
