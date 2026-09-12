#include "power_key.h"

#include <assert.h>

int main(void)
{
    struct moko_power_key_state state = {0};

    assert(MOKO_POWER_KEY_HOLD_MS == 5000);

    assert(moko_power_key_release(&state) == MOKO_POWER_KEY_ACTION_NONE);
    assert(moko_power_key_press(&state) == MOKO_POWER_KEY_ACTION_START_TIMER);
    assert(state.pressed && !state.menu_sent);
    assert(moko_power_key_press(&state) == MOKO_POWER_KEY_ACTION_NONE);
    assert(moko_power_key_release(&state) == MOKO_POWER_KEY_ACTION_CANCEL_TIMER);
    assert(!state.pressed && !state.menu_sent);

    assert(moko_power_key_press(&state) == MOKO_POWER_KEY_ACTION_START_TIMER);
    assert(moko_power_key_timer(&state) == MOKO_POWER_KEY_ACTION_SHOW_MENU);
    assert(state.pressed && state.menu_sent);
    assert(moko_power_key_timer(&state) == MOKO_POWER_KEY_ACTION_NONE);
    assert(moko_power_key_release(&state) == MOKO_POWER_KEY_ACTION_NONE);
    assert(!state.pressed && state.menu_sent);

    return 0;
}
