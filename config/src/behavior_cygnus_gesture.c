#define DT_DRV_COMPAT zmk_behavior_cygnus_gesture

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/events/position_state_changed.h>

#include <dt-bindings/zmk/cygnus_gesture.h>
#include <cygnus_gesture.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct cygnus_gesture_config {
    int32_t threshold;
    int32_t ratio_numerator;
    int32_t ratio_denominator;
    uint8_t win_layer;
    const struct zmk_behavior_binding *bindings;
};

enum cygnus_gesture_binding_group {
    CYG_BINDINGS_D_MAC = 0,
    CYG_BINDINGS_D_WIN,
    CYG_BINDINGS_M_MAC,
    CYG_BINDINGS_M_WIN,
    CYG_BINDINGS_G_MAC,
    CYG_BINDINGS_G_WIN,
    CYG_BINDINGS_R_MAC,
    CYG_BINDINGS_R_WIN,
    CYG_BINDINGS_GROUP_COUNT,
};

static int invoke_tap(const struct zmk_behavior_binding *binding, uint32_t position) {
    struct zmk_behavior_binding_event event = {
        .position = position,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    int ret = zmk_behavior_invoke_binding(binding, event, true);
    if (ret < 0) {
        return ret;
    }

    event.timestamp = k_uptime_get();
    return zmk_behavior_invoke_binding(binding, event, false);
}

static const struct zmk_behavior_binding *binding_table_for(const struct cygnus_gesture_config *cfg,
                                                            uint8_t kind, bool windows) {
    uint8_t group;

    switch (kind) {
    case CYG_GESTURE_D:
        group = windows ? CYG_BINDINGS_D_WIN : CYG_BINDINGS_D_MAC;
        break;
    case CYG_GESTURE_M:
        group = windows ? CYG_BINDINGS_M_WIN : CYG_BINDINGS_M_MAC;
        break;
    case CYG_GESTURE_G:
        group = windows ? CYG_BINDINGS_G_WIN : CYG_BINDINGS_G_MAC;
        break;
    case CYG_GESTURE_R:
        group = windows ? CYG_BINDINGS_R_WIN : CYG_BINDINGS_R_MAC;
        break;
    default:
        return NULL;
    }

    return &cfg->bindings[group * CYGNUS_GESTURE_BINDING_COUNT];
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }

    const struct cygnus_gesture_config *cfg = dev->config;
    cygnus_gesture_begin(binding->param1, cfg->threshold, cfg->ratio_numerator,
                         cfg->ratio_denominator);

    LOG_DBG("Cygnus gesture begin kind=%d pos=%d", binding->param1, event.position);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        cygnus_gesture_cancel();
        return -ENODEV;
    }

    const struct cygnus_gesture_config *cfg = dev->config;
    struct cygnus_gesture_snapshot snapshot = cygnus_gesture_finish();

    if (!snapshot.active) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    // G is a pan gesture. If pan already emitted scroll events, do not also fire MB3.
    if (snapshot.kind == CYG_GESTURE_G && snapshot.pan_started) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    uint8_t direction = cygnus_gesture_direction(&snapshot);
    if (direction >= CYGNUS_GESTURE_BINDING_COUNT) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    bool windows = zmk_keymap_layer_active(cfg->win_layer);
    const struct zmk_behavior_binding *table = binding_table_for(cfg, snapshot.kind, windows);
    if (table == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    LOG_DBG("Cygnus gesture fire kind=%d dir=%d host=%s dx=%d dy=%d", snapshot.kind, direction,
            windows ? "win" : "mac", snapshot.dx, snapshot.dy);

    return invoke_tap(&table[direction], event.position);
}

static const struct behavior_driver_api cygnus_gesture_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define CYG_BINDING(idx, inst) ZMK_KEYMAP_EXTRACT_BINDING(idx, DT_DRV_INST(inst))

#define CYG_BINDINGS_TOTAL (CYG_BINDINGS_GROUP_COUNT * CYGNUS_GESTURE_BINDING_COUNT)

#define CYG_INST(n)                                                                                \
    static const struct zmk_behavior_binding cyg_bindings_##n[] = {                                \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), CYG_BINDING, (, ), n)};                             \
    BUILD_ASSERT(ARRAY_SIZE(cyg_bindings_##n) == CYG_BINDINGS_TOTAL,                               \
                 "cy_gesture bindings must contain 8 groups of 5 bindings");                      \
    static const struct cygnus_gesture_config cyg_config_##n = {                                   \
        .threshold = DT_INST_PROP(n, threshold),                                                    \
        .ratio_numerator = DT_INST_PROP(n, ratio_numerator),                                       \
        .ratio_denominator = DT_INST_PROP(n, ratio_denominator),                                   \
        .win_layer = DT_INST_PROP(n, win_layer),                                                   \
        .bindings = cyg_bindings_##n,                                                             \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &cyg_config_##n, POST_KERNEL,                     \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &cygnus_gesture_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CYG_INST)
