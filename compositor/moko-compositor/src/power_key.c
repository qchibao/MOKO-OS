#include "power_key.h"

#include <stddef.h>

enum moko_power_key_action moko_power_key_press(struct moko_power_key_state *state)
{
    if (state == NULL || state->pressed)
        return MOKO_POWER_KEY_ACTION_NONE;

    state->pressed = true;
    state->menu_sent = false;
    return MOKO_POWER_KEY_ACTION_START_TIMER;
}

enum moko_power_key_action moko_power_key_release(struct moko_power_key_state *state)
{
    if (state == NULL || !state->pressed)
        return MOKO_POWER_KEY_ACTION_NONE;

    state->pressed = false;
    if (state->menu_sent)
        return MOKO_POWER_KEY_ACTION_NONE;
    return MOKO_POWER_KEY_ACTION_CANCEL_TIMER;
}

enum moko_power_key_action moko_power_key_timer(struct moko_power_key_state *state)
{
    if (state == NULL || !state->pressed || state->menu_sent)
        return MOKO_POWER_KEY_ACTION_NONE;

    state->menu_sent = true;
    return MOKO_POWER_KEY_ACTION_SHOW_MENU;
}
