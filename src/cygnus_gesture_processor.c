#define DT_DRV_COMPAT cygnus_input_processor_gesture

#include <errno.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CYG_KIND_NONE 0
#define CYG_KIND_D 1
#define CYG_KIND_M 2
#define CYG_KIND_N 3
#define CYG_KIND_R 4

#define CYG_DIR_NONE 0
#define CYG_DIR_LEFT 1
#define CYG_DIR_RIGHT 2
#define CYG_DIR_UP 3
#define CYG_DIR_DOWN 4
#define CYG_BINDING_COUNT 5

struct cygnus_gesture_processor_config {
    int32_t threshold;
    int32_t ratio_numerator;
    int32_t ratio_denominator;
    int32_t tap_ms;
    uint8_t win_layer;
    uint8_t automouse_layer;
    uint8_t win_automouse_layer;
    uint32_t n_position;
    uint32_t r_position;
    uint32_t d_position;
    uint32_t m_position;
    const struct zmk_behavior_binding *d_mac_bindings;
    const struct zmk_behavior_binding *d_win_bindings;
    const struct zmk_behavior_binding *m_mac_bindings;
    const struct zmk_behavior_binding *m_win_bindings;
    const struct zmk_behavior_binding *r_mac_bindings;
    const struct zmk_behavior_binding *r_win_bindings;
};

struct cygnus_gesture_processor_data {
    bool active;
    bool fired;
    bool pan_started;
    uint8_t kind;
    uint32_t position;
    int32_t dx;
    int32_t dy;
};

static uint8_t kind_for_position(const struct cygnus_gesture_processor_config *cfg,
                                 uint32_t position) {
    if (position == cfg->d_position) {
        return CYG_KIND_D;
    }
    if (position == cfg->m_position) {
        return CYG_KIND_M;
    }
    if (position == cfg->n_position) {
        return CYG_KIND_N;
    }
    if (position == cfg->r_position) {
        return CYG_KIND_R;
    }
    return CYG_KIND_NONE;
}

static const struct zmk_behavior_binding *binding_table_for(
    const struct cygnus_gesture_processor_config *cfg, uint8_t kind, bool windows) {
    switch (kind) {
    case CYG_KIND_D:
        return windows ? cfg->d_win_bindings : cfg->d_mac_bindings;
    case CYG_KIND_M:
        return windows ? cfg->m_win_bindings : cfg->m_mac_bindings;
    case CYG_KIND_R:
        return windows ? cfg->r_win_bindings : cfg->r_mac_bindings;
    default:
        return NULL;
    }
}

static uint8_t direction_for(int32_t dx, int32_t dy, int32_t threshold, int32_t ratio_num,
                             int32_t ratio_den) {
    int32_t ax = abs(dx);
    int32_t ay = abs(dy);

    if (ax < threshold && ay < threshold) {
        return CYG_DIR_NONE;
    }

    if (ax * ratio_den > ay * ratio_num) {
        return dx < 0 ? CYG_DIR_LEFT : CYG_DIR_RIGHT;
    }

    if (ay * ratio_den > ax * ratio_num) {
        return dy < 0 ? CYG_DIR_UP : CYG_DIR_DOWN;
    }

    return CYG_DIR_NONE;
}

static int queue_binding(const struct zmk_behavior_binding *binding, uint32_t position,
                         uint32_t tap_ms) {
    if (binding == NULL || binding->behavior_dev == NULL) {
        return -EINVAL;
    }

    struct zmk_behavior_binding_event event = {
        .position = position,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    int ret = zmk_behavior_queue_add(&event, *binding, true, tap_ms);
    if (ret < 0) {
        return ret;
    }
    return zmk_behavior_queue_add(&event, *binding, false, 0);
}

static void begin_gesture(struct cygnus_gesture_processor_data *data, uint8_t kind,
                          uint32_t position) {
    data->active = true;
    data->fired = false;
    data->pan_started = false;
    data->kind = kind;
    data->position = position;
    data->dx = 0;
    data->dy = 0;
}

static void finish_gesture(struct cygnus_gesture_processor_data *data) {
    data->active = false;
    data->fired = false;
    data->pan_started = false;
    data->kind = CYG_KIND_NONE;
    data->position = 0;
    data->dx = 0;
    data->dy = 0;
}

static int handle_position_state_changed(const struct device *dev, const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct cygnus_gesture_processor_config *cfg = dev->config;
    struct cygnus_gesture_processor_data *data = dev->data;

    if (!ev->state) {
        if (data->active && ev->position == data->position) {
            finish_gesture(data);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (!zmk_keymap_layer_active(cfg->automouse_layer) &&
        !zmk_keymap_layer_active(cfg->win_automouse_layer)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t kind = kind_for_position(cfg, ev->position);
    if (kind == CYG_KIND_NONE) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    begin_gesture(data, kind, ev->position);
    return ZMK_EV_EVENT_BUBBLE;
}

#define DISPATCH_POSITION_EVENT(inst)                                                             \
    {                                                                                              \
        int err = handle_position_state_changed(DEVICE_DT_INST_GET(inst), eh);                     \
        if (err < 0) {                                                                             \
            return err;                                                                            \
        }                                                                                          \
    }

static int handle_position_event_dispatcher(const zmk_event_t *eh) {
    DT_INST_FOREACH_STATUS_OKAY(DISPATCH_POSITION_EVENT)
    return ZMK_EV_EVENT_BUBBLE;
}

static int cygnus_gesture_processor_handle_event(const struct device *dev,
                                                 struct input_event *event, uint32_t param1,
                                                 uint32_t param2,
                                                 struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct cygnus_gesture_processor_config *cfg = dev->config;
    struct cygnus_gesture_processor_data *data = dev->data;

    if (!data->active) {
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
    data->dx += dx;
    data->dy += dy;

    if (data->kind == CYG_KIND_N) {
        if (abs(data->dx) >= cfg->threshold || abs(data->dy) >= cfg->threshold) {
            data->pan_started = true;
        }

        if (data->pan_started) {
            event->code = event->code == INPUT_REL_X ? INPUT_REL_HWHEEL : INPUT_REL_WHEEL;
            return ZMK_INPUT_PROC_CONTINUE;
        }

        event->value = 0;
        return ZMK_INPUT_PROC_STOP;
    }

    if (!data->fired) {
        uint8_t direction = direction_for(data->dx, data->dy, cfg->threshold,
                                          cfg->ratio_numerator, cfg->ratio_denominator);
        if (direction != CYG_DIR_NONE) {
            bool windows = zmk_keymap_layer_active(cfg->win_layer);
            const struct zmk_behavior_binding *table = binding_table_for(cfg, data->kind, windows);
            if (table != NULL && direction < CYG_BINDING_COUNT) {
                int ret = queue_binding(&table[direction], data->position, MAX(cfg->tap_ms, 0));
                if (ret >= 0) {
                    data->fired = true;
                }
            }
        }
    }

    event->value = 0;
    return ZMK_INPUT_PROC_STOP;
}

static struct zmk_input_processor_driver_api cygnus_gesture_processor_api = {
    .handle_event = cygnus_gesture_processor_handle_event,
};

#define CYG_BINDING_FROM_PROP(idx, inst, prop)                                                     \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_PHANDLE_BY_IDX(DT_DRV_INST(inst), prop, idx)),           \
        .param1 = COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(DT_DRV_INST(inst), prop, idx, param1), (0),   \
                              (DT_PHA_BY_IDX(DT_DRV_INST(inst), prop, idx, param1))),              \
        .param2 = COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(DT_DRV_INST(inst), prop, idx, param2), (0),   \
                              (DT_PHA_BY_IDX(DT_DRV_INST(inst), prop, idx, param2))),              \
    }

#define CYG_D_MAC_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, d_mac_bindings)
#define CYG_D_WIN_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, d_win_bindings)
#define CYG_M_MAC_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, m_mac_bindings)
#define CYG_M_WIN_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, m_win_bindings)
#define CYG_R_MAC_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, r_mac_bindings)
#define CYG_R_WIN_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, r_win_bindings)

#define CYG_ASSERT_BINDINGS_LEN(n, prop)                                                           \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, prop) == CYG_BINDING_COUNT,                                   \
                 #prop " must contain exactly 5 bindings: none, left, right, up, down")

#define CYGNUS_GESTURE_PROCESSOR_INST(n)                                                           \
    static const struct zmk_behavior_binding cyg_d_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, d_mac_bindings), CYG_D_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_d_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, d_win_bindings), CYG_D_WIN_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_m_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, m_mac_bindings), CYG_M_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_m_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, m_win_bindings), CYG_M_WIN_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_r_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, r_mac_bindings), CYG_R_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_r_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, r_win_bindings), CYG_R_WIN_BINDING, (, ), n)};                 \
    CYG_ASSERT_BINDINGS_LEN(n, d_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, d_win_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, m_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, m_win_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, r_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, r_win_bindings);                                                    \
    static struct cygnus_gesture_processor_data cygnus_gesture_data_##n = {};                      \
    static const struct cygnus_gesture_processor_config cygnus_gesture_config_##n = {              \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .ratio_numerator = DT_INST_PROP(n, ratio_numerator),                                       \
        .ratio_denominator = DT_INST_PROP(n, ratio_denominator),                                   \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
        .win_layer = DT_INST_PROP(n, win_layer),                                                   \
        .automouse_layer = DT_INST_PROP(n, automouse_layer),                                       \
        .win_automouse_layer = DT_INST_PROP(n, win_automouse_layer),                               \
        .n_position = DT_INST_PROP(n, n_position),                                                 \
        .r_position = DT_INST_PROP(n, r_position),                                                 \
        .d_position = DT_INST_PROP(n, d_position),                                                 \
        .m_position = DT_INST_PROP(n, m_position),                                                 \
        .d_mac_bindings = cyg_d_mac_bindings_##n,                                                  \
        .d_win_bindings = cyg_d_win_bindings_##n,                                                  \
        .m_mac_bindings = cyg_m_mac_bindings_##n,                                                  \
        .m_win_bindings = cyg_m_win_bindings_##n,                                                  \
        .r_mac_bindings = cyg_r_mac_bindings_##n,                                                  \
        .r_win_bindings = cyg_r_win_bindings_##n,                                                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &cygnus_gesture_data_##n, &cygnus_gesture_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &cygnus_gesture_processor_api);

ZMK_LISTENER(cygnus_gesture_processor, handle_position_event_dispatcher);
ZMK_SUBSCRIPTION(cygnus_gesture_processor, zmk_position_state_changed);

DT_INST_FOREACH_STATUS_OKAY(CYGNUS_GESTURE_PROCESSOR_INST)
