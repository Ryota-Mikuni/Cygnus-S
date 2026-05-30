#include <stdlib.h>
#include <string.h>

#include <dt-bindings/zmk/cygnus_gesture.h>
#include <cygnus_gesture.h>

static struct cygnus_gesture_snapshot state;

void cygnus_gesture_begin(uint8_t kind, int32_t threshold, int32_t ratio_numerator,
                          int32_t ratio_denominator) {
    memset(&state, 0, sizeof(state));
    state.active = true;
    state.kind = kind;
    state.threshold = threshold > 0 ? threshold : 70;
    state.ratio_numerator = ratio_numerator > 0 ? ratio_numerator : 13;
    state.ratio_denominator = ratio_denominator > 0 ? ratio_denominator : 10;
}

void cygnus_gesture_cancel(void) { memset(&state, 0, sizeof(state)); }

bool cygnus_gesture_active(void) { return state.active; }

uint8_t cygnus_gesture_kind(void) { return state.kind; }

void cygnus_gesture_add_delta(int32_t dx, int32_t dy) {
    if (!state.active) {
        return;
    }

    state.dx += dx;
    state.dy += dy;

    if (state.kind == CYG_GESTURE_G) {
        if (abs(state.dx) >= state.threshold || abs(state.dy) >= state.threshold) {
            state.pan_started = true;
        }
    }
}

bool cygnus_gesture_should_pan(void) {
    return state.active && state.kind == CYG_GESTURE_G && state.pan_started;
}

struct cygnus_gesture_snapshot cygnus_gesture_finish(void) {
    struct cygnus_gesture_snapshot snapshot = state;
    memset(&state, 0, sizeof(state));
    return snapshot;
}

uint8_t cygnus_gesture_direction(const struct cygnus_gesture_snapshot *snapshot) {
    int32_t ax = abs(snapshot->dx);
    int32_t ay = abs(snapshot->dy);
    int32_t threshold = snapshot->threshold > 0 ? snapshot->threshold : 70;
    int32_t ratio_num = snapshot->ratio_numerator > 0 ? snapshot->ratio_numerator : 13;
    int32_t ratio_den = snapshot->ratio_denominator > 0 ? snapshot->ratio_denominator : 10;

    if (ax < threshold && ay < threshold) {
        return CYGNUS_GESTURE_DIR_NONE;
    }

    if (ax * ratio_den > ay * ratio_num) {
        return snapshot->dx < 0 ? CYGNUS_GESTURE_DIR_LEFT : CYGNUS_GESTURE_DIR_RIGHT;
    }

    if (ay * ratio_den > ax * ratio_num) {
        return snapshot->dy < 0 ? CYGNUS_GESTURE_DIR_UP : CYGNUS_GESTURE_DIR_DOWN;
    }

    return CYGNUS_GESTURE_DIR_NONE;
}
