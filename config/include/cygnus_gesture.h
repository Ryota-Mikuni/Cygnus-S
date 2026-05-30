#pragma once

#include <zephyr/types.h>
#include <stdbool.h>

#define CYGNUS_GESTURE_DIR_NONE 0
#define CYGNUS_GESTURE_DIR_LEFT 1
#define CYGNUS_GESTURE_DIR_RIGHT 2
#define CYGNUS_GESTURE_DIR_UP 3
#define CYGNUS_GESTURE_DIR_DOWN 4

#define CYGNUS_GESTURE_BINDING_COUNT 5

struct cygnus_gesture_snapshot {
    bool active;
    uint8_t kind;
    int32_t dx;
    int32_t dy;
    bool pan_started;
    int32_t threshold;
    int32_t ratio_numerator;
    int32_t ratio_denominator;
};

void cygnus_gesture_begin(uint8_t kind, int32_t threshold, int32_t ratio_numerator,
                          int32_t ratio_denominator);
void cygnus_gesture_cancel(void);
bool cygnus_gesture_active(void);
uint8_t cygnus_gesture_kind(void);
void cygnus_gesture_add_delta(int32_t dx, int32_t dy);
bool cygnus_gesture_should_pan(void);
struct cygnus_gesture_snapshot cygnus_gesture_finish(void);
uint8_t cygnus_gesture_direction(const struct cygnus_gesture_snapshot *snapshot);
