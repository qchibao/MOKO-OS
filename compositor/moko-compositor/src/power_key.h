#pragma once

#include <stdbool.h>

#define MOKO_POWER_KEY_HOLD_MS 5000

enum moko_power_key_action {
    MOKO_POWER_KEY_ACTION_NONE,
    MOKO_POWER_KEY_ACTION_START_TIMER,
    MOKO_POWER_KEY_ACTION_CANCEL_TIMER,
    MOKO_POWER_KEY_ACTION_SHOW_MENU,
};

struct moko_power_key_state {
    bool pressed;
    bool menu_sent;
};

enum moko_power_key_action moko_power_key_press(struct moko_power_key_state *state);
enum moko_power_key_action moko_power_key_release(struct moko_power_key_state *state);
enum moko_power_key_action moko_power_key_timer(struct moko_power_key_state *state);
