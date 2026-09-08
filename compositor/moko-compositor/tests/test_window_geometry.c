#include "window_geometry.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    const struct moko_rect output = {0, 0, 1440, 900};
    const struct moko_work_area_config config = {48, 112, 320, 240};
    const struct moko_rect area = moko_work_area(output, config);
    assert(area.x == 0);
    assert(area.y == 48);
    assert(area.width == 1440);
    assert(area.height == 740);

    const struct moko_rect centered = moko_centered_rect(area, 800, 500, 0);
    assert(centered.x == 320);
    assert(centered.y == 168);
    assert(centered.width == 800);
    assert(centered.height == 500);

    const struct moko_rect constrained = moko_centered_rect(area, 1040, 680, 0);
    assert(constrained.x == 200);
    assert(constrained.y == 80);
    assert(constrained.width == 1040);
    assert(constrained.height == 676);

    const struct moko_rect cascaded = moko_centered_rect(area, 1040, 680, 28);
    assert(cascaded.x == 228);
    assert(cascaded.y == 108);
    assert(cascaded.width == 1040);
    assert(cascaded.height == 676);

    const struct moko_rect small_output = {10, 20, 300, 200};
    const struct moko_rect small_area = moko_work_area(small_output, config);
    assert(small_area.x == 10);
    assert(small_area.y == 20);
    assert(small_area.width == 300);
    assert(small_area.height == 200);
    const struct moko_rect small_window = moko_centered_rect(small_area, 960, 640, 0);
    assert(small_window.x >= small_area.x);
    assert(small_window.y >= small_area.y);
    assert(small_window.x + small_window.width <= small_area.x + small_area.width);
    assert(small_window.y + small_window.height <= small_area.y + small_area.height);

    const struct moko_rect invalid_area = moko_work_area(
        (struct moko_rect){0, 0, 0, 100}, config);
    assert(!moko_rect_valid(invalid_area));

    const struct moko_rect left = moko_snap_rect(area, false);
    const struct moko_rect right = moko_snap_rect(area, true);
    assert(left.x == 0 && left.width == 720 && left.height == 740);
    assert(right.x == 720 && right.width == 720 && right.height == 740);
    assert(moko_rect_valid(left));
    assert(!moko_rect_valid((struct moko_rect){0, 0, 0, 100}));

    const struct moko_rect translated = moko_translate_rect(centered, 42.9, -18.4);
    assert(translated.x == centered.x + 42);
    assert(translated.y == centered.y - 18);
    assert(translated.width == centered.width && translated.height == centered.height);

    puts("MOKO compositor geometry tests passed.");
    return 0;
}
