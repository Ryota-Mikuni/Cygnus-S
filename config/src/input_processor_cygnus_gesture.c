#define DT_DRV_COMPAT zmk_input_processor_cygnus_gesture

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
#include <dt-bindings/zmk/cygnus_gesture.h>
#include <cygnus_gesture.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int cygnus_gesture_processor_handle_event(const struct device *dev,
                                                 struct input_event *event, uint32_t param1,
                                                 uint32_t param2,
                                                 struct zmk_input_processor_state *state) {
    if (!cygnus_gesture_active()) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t dx = event->code == INPUT_REL_X ? event->value : 0;
    int32_t dy = event->code == INPUT_REL_Y ? event->value : 0;
    cygnus_gesture_add_delta(dx, dy);

    // G is the pan gesture. Once movement crosses the threshold, convert the
    // ball motion into scroll events until the G key is released.
    if (cygnus_gesture_kind() == CYG_GESTURE_G && cygnus_gesture_should_pan()) {
        if (event->code == INPUT_REL_X) {
            event->code = INPUT_REL_HWHEEL;
        } else {
            event->code = INPUT_REL_WHEEL;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // D/M gestures are discrete commands. Swallow pointer motion while deciding
    // the gesture direction. G also swallows motion before pan threshold is met.
    event->value = 0;
    return ZMK_INPUT_PROC_STOP;
}

static struct zmk_input_processor_driver_api cygnus_gesture_processor_driver_api = {
    .handle_event = cygnus_gesture_processor_handle_event,
};

#define CYG_PROC_INST(n)                                                                           \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &cygnus_gesture_processor_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CYG_PROC_INST)
