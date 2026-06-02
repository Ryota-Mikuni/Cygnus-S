#define DT_DRV_COMPAT cygnus_input_processor_gesture

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>

#include <zmk/behavior.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

enum cygnus_gesture_direction {
    CYGNUS_GESTURE_RIGHT = 0,
    CYGNUS_GESTURE_LEFT = 1,
    CYGNUS_GESTURE_UP = 2,
    CYGNUS_GESTURE_DOWN = 3,
};

struct cygnus_gesture_processor_config {
    uint8_t index;
    int16_t tick;
    int32_t wait_ms;
    int32_t tap_ms;
    const struct zmk_behavior_binding *bindings;
};

struct cygnus_gesture_processor_data {
    int16_t x;
    int16_t y;
    int64_t last_triggered_at;
    const struct device *dev;
    struct k_work_delayable press_work;
    struct k_work_delayable release_work;
    enum cygnus_gesture_direction pending_direction;
    uint32_t pending_position;
    bool pending;
};

static int invoke_pending_binding(const struct cygnus_gesture_processor_config *cfg,
                                  struct cygnus_gesture_processor_data *data, bool pressed) {
    enum cygnus_gesture_direction direction = data->pending_direction;
    const struct zmk_behavior_binding *binding = &cfg->bindings[direction];
    struct zmk_behavior_binding_event event = {
        .position = data->pending_position,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    return zmk_behavior_invoke_binding(binding, event, pressed);
}

static void release_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct cygnus_gesture_processor_data *data =
        CONTAINER_OF(dwork, struct cygnus_gesture_processor_data, release_work);

    if (!data->pending || data->dev == NULL) {
        return;
    }

    const struct cygnus_gesture_processor_config *cfg = data->dev->config;
    invoke_pending_binding(cfg, data, false);
    data->pending = false;
}

static void press_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct cygnus_gesture_processor_data *data =
        CONTAINER_OF(dwork, struct cygnus_gesture_processor_data, press_work);

    if (!data->pending || data->dev == NULL) {
        return;
    }

    const struct cygnus_gesture_processor_config *cfg = data->dev->config;
    int ret = invoke_pending_binding(cfg, data, true);
    if (ret < 0) {
        data->pending = false;
        return;
    }

    k_work_schedule(&data->release_work, K_MSEC(MAX(cfg->tap_ms, 0)));
}

static int cygnus_gesture_processor_handle_event(const struct device *dev,
                                                 struct input_event *event, uint32_t param1,
                                                 uint32_t param2,
                                                 struct zmk_input_processor_state *state) {
    struct cygnus_gesture_processor_data *data = dev->data;
    const struct cygnus_gesture_processor_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    switch (event->code) {
    case INPUT_REL_X:
        data->x += event->value;
        event->value = 0;
        break;
    case INPUT_REL_Y:
        data->y += event->value;
        event->value = 0;
        break;
    default:
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (!event->sync) {
        return ZMK_INPUT_PROC_STOP;
    }

    const int16_t abs_x = abs(data->x);
    const int16_t abs_y = abs(data->y);
    if (abs_x < cfg->tick && abs_y < cfg->tick) {
        return ZMK_INPUT_PROC_STOP;
    }

    int64_t now = k_uptime_get();
    if (cfg->wait_ms > 0 && data->last_triggered_at > 0 &&
        now - data->last_triggered_at < cfg->wait_ms) {
        data->x = 0;
        data->y = 0;
        return ZMK_INPUT_PROC_STOP;
    }

    enum cygnus_gesture_direction direction;
    if (abs_x >= abs_y) {
        direction = data->x > 0 ? CYGNUS_GESTURE_RIGHT : CYGNUS_GESTURE_LEFT;
    } else {
        direction = data->y < 0 ? CYGNUS_GESTURE_UP : CYGNUS_GESTURE_DOWN;
    }

    data->x = 0;
    data->y = 0;
    data->last_triggered_at = now;

    if (data->pending) {
        return ZMK_INPUT_PROC_STOP;
    }

    data->pending_direction = direction;
    data->pending_position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
        state->input_device_index, cfg->index);
    data->pending = true;
    k_work_schedule(&data->press_work, K_NO_WAIT);

    return ZMK_INPUT_PROC_STOP;
}

static struct zmk_input_processor_driver_api cygnus_gesture_processor_api = {
    .handle_event = cygnus_gesture_processor_handle_event,
};

static int cygnus_gesture_processor_init(const struct device *dev) {
    struct cygnus_gesture_processor_data *data = dev->data;

    data->dev = dev;
    k_work_init_delayable(&data->press_work, press_work_handler);
    k_work_init_delayable(&data->release_work, release_work_handler);

    return 0;
}

#define CYGNUS_GESTURE_PROCESSOR_INST(n)                                                           \
    static const struct zmk_behavior_binding cygnus_gesture_bindings_##n[] = {                     \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))}; \
    BUILD_ASSERT(ARRAY_SIZE(cygnus_gesture_bindings_##n) == 4,                                     \
                 "cygnus,input-processor-gesture requires 4 bindings: right, left, up, down");    \
    static struct cygnus_gesture_processor_data cygnus_gesture_data_##n = {};                      \
    static const struct cygnus_gesture_processor_config cygnus_gesture_config_##n = {              \
        .index = n,                                                                                \
        .tick = DT_INST_PROP(n, tick),                                                             \
        .wait_ms = DT_INST_PROP(n, wait_ms),                                                       \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
        .bindings = cygnus_gesture_bindings_##n,                                                   \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, &cygnus_gesture_processor_init, NULL, &cygnus_gesture_data_##n,        \
                          &cygnus_gesture_config_##n, POST_KERNEL,                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &cygnus_gesture_processor_api);

DT_INST_FOREACH_STATUS_OKAY(CYGNUS_GESTURE_PROCESSOR_INST)
