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
    const struct zmk_behavior_binding *d_mac_bindings;
    const struct zmk_behavior_binding *d_win_bindings;
    const struct zmk_behavior_binding *m_mac_bindings;
    const struct zmk_behavior_binding *m_win_bindings;
    const struct zmk_behavior_binding *g_mac_bindings;
    const struct zmk_behavior_binding *g_win_bindings;
    const struct zmk_behavior_binding *r_mac_bindings;
    const struct zmk_behavior_binding *r_win_bindings;
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
    switch (kind) {
    case CYG_GESTURE_D:
        return windows ? cfg->d_win_bindings : cfg->d_mac_bindings;
    case CYG_GESTURE_M:
        return windows ? cfg->m_win_bindings : cfg->m_mac_bindings;
    case CYG_GESTURE_G:
        return windows ? cfg->g_win_bindings : cfg->g_mac_bindings;
    case CYG_GESTURE_R:
        return windows ? cfg->r_win_bindings : cfg->r_mac_bindings;
    default:
        return NULL;
    }
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
#define CYG_G_MAC_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, g_mac_bindings)
#define CYG_G_WIN_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, g_win_bindings)
#define CYG_R_MAC_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, r_mac_bindings)
#define CYG_R_WIN_BINDING(idx, inst) CYG_BINDING_FROM_PROP(idx, inst, r_win_bindings)

#define CYG_ASSERT_BINDINGS_LEN(n, prop)                                                           \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, prop) == CYGNUS_GESTURE_BINDING_COUNT,                        \
                 #prop " must contain exactly 5 bindings: none,left,right,up,down")

#define CYG_INST(n)                                                                                \
    static const struct zmk_behavior_binding cyg_d_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, d_mac_bindings), CYG_D_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_d_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, d_win_bindings), CYG_D_WIN_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_m_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, m_mac_bindings), CYG_M_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_m_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, m_win_bindings), CYG_M_WIN_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_g_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, g_mac_bindings), CYG_G_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_g_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, g_win_bindings), CYG_G_WIN_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_r_mac_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, r_mac_bindings), CYG_R_MAC_BINDING, (, ), n)};                 \
    static const struct zmk_behavior_binding cyg_r_win_bindings_##n[] = {                          \
        LISTIFY(DT_INST_PROP_LEN(n, r_win_bindings), CYG_R_WIN_BINDING, (, ), n)};                 \
    CYG_ASSERT_BINDINGS_LEN(n, d_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, d_win_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, m_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, m_win_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, g_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, g_win_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, r_mac_bindings);                                                    \
    CYG_ASSERT_BINDINGS_LEN(n, r_win_bindings);                                                    \
    static const struct cygnus_gesture_config cyg_config_##n = {                                   \
        .threshold = DT_INST_PROP(n, threshold),                                                    \
        .ratio_numerator = DT_INST_PROP(n, ratio_numerator),                                       \
        .ratio_denominator = DT_INST_PROP(n, ratio_denominator),                                   \
        .win_layer = DT_INST_PROP(n, win_layer),                                                   \
        .d_mac_bindings = cyg_d_mac_bindings_##n,                                                  \
        .d_win_bindings = cyg_d_win_bindings_##n,                                                  \
        .m_mac_bindings = cyg_m_mac_bindings_##n,                                                  \
        .m_win_bindings = cyg_m_win_bindings_##n,                                                  \
        .g_mac_bindings = cyg_g_mac_bindings_##n,                                                  \
        .g_win_bindings = cyg_g_win_bindings_##n,                                                  \
        .r_mac_bindings = cyg_r_mac_bindings_##n,                                                  \
        .r_win_bindings = cyg_r_win_bindings_##n,                                                  \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &cyg_config_##n, POST_KERNEL,                     \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &cygnus_gesture_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CYG_INST)
