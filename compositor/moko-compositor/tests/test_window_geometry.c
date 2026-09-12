#include "window_geometry.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/*
 * Regression coverage for the three-finger drag pointer fix.
 *
 * moko_translate_rect() truncates to whole pixels, so a gesture whose updates
 * each carry a sub-pixel delta moves the window not at all while a cursor fed
 * the raw deltas keeps drifting away from it. The compositor therefore measures
 * the window's actual displacement and moves the pointer by exactly that.
 * Model both loops and assert that only the measured one stays glued.
 */
struct drag_result {
    struct moko_rect window;
    double cursor_x;
    double cursor_y;
};

static struct drag_result gesture_drag(struct moko_rect window, double cursor_x,
                                      double cursor_y, const double *deltas,
                                      size_t updates, bool measured)
{
    for (size_t i = 0; i < updates; i++) {
        const struct moko_rect before = window;
        const struct moko_rect next =
            moko_translate_rect(before, deltas[i * 2], deltas[i * 2 + 1]);

        /* apply_geometry() leaves the window alone when the rect is invalid. */
        if (moko_rect_valid(next))
            window = next;

        if (measured) {
            cursor_x += window.x - before.x;
            cursor_y += window.y - before.y;
        } else {
            cursor_x += deltas[i * 2];
            cursor_y += deltas[i * 2 + 1];
        }
    }
    return (struct drag_result){window, cursor_x, cursor_y};
}

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

    /*
     * Slow drag: 60 updates of +/-0.75 px, the case where per-update
     * truncation is total. The window never moves, so a correct pointer must
     * not move either. 0.75 is exactly representable in binary, so the
     * accumulated totals below are exact rather than approximate.
     */
    double slow_deltas[120];
    for (size_t i = 0; i < 60; i++) {
        slow_deltas[i * 2] = 0.75;
        slow_deltas[i * 2 + 1] = -0.75;
    }

    const struct drag_result slow_naive =
        gesture_drag(centered, 400.0, 300.0, slow_deltas, 60, false);
    assert(slow_naive.window.x == centered.x);
    assert(slow_naive.window.y == centered.y);
    assert(slow_naive.cursor_x == 445.0); /* drifted 45 px off a static window */
    assert(slow_naive.cursor_y == 255.0);

    const struct drag_result slow_measured =
        gesture_drag(centered, 400.0, 300.0, slow_deltas, 60, true);
    assert(slow_measured.window.x == centered.x);
    assert(slow_measured.window.y == centered.y);
    assert(slow_measured.cursor_x == 400.0); /* still exactly where it started */
    assert(slow_measured.cursor_y == 300.0);

    /*
     * Fast drag: whole-pixel deltas, where truncation is lossless. Both loops
     * must agree, so the fix does not regress the case that already worked.
     */
    double fast_deltas[20];
    for (size_t i = 0; i < 10; i++) {
        fast_deltas[i * 2] = 12.0;
        fast_deltas[i * 2 + 1] = -5.0;
    }
    const struct drag_result fast_measured =
        gesture_drag(centered, 400.0, 300.0, fast_deltas, 10, true);
    const struct drag_result fast_naive =
        gesture_drag(centered, 400.0, 300.0, fast_deltas, 10, false);
    assert(fast_measured.window.x == centered.x + 120);
    assert(fast_measured.window.y == centered.y - 50);
    assert(fast_measured.cursor_x == fast_naive.cursor_x);
    assert(fast_measured.cursor_y == fast_naive.cursor_y);

    /*
     * Mixed drag: the realistic case, where some updates cross a pixel
     * boundary and some do not. Truncation loses the sub-pixel remainder every
     * time, and that remainder accumulates on a pointer fed the raw deltas.
     */
    const double mixed_deltas[12] = {
        0.50,  0.50, 0.25, 0.25, 1.50, 1.50,
        0.75, -0.75, 2.50, -2.50, 0.50, 0.25,
    };
    const struct drag_result mixed_measured =
        gesture_drag(centered, 400.0, 300.0, mixed_deltas, 6, true);
    const struct drag_result mixed_naive =
        gesture_drag(centered, 400.0, 300.0, mixed_deltas, 6, false);

    /* The window moved by the truncated totals: +3, -1. */
    assert(mixed_measured.window.x == centered.x + 3);
    assert(mixed_measured.window.y == centered.y - 1);
    assert(mixed_measured.cursor_x == 403.0);
    assert(mixed_measured.cursor_y == 299.0);

    /* The raw deltas summed to +6.00, -0.75 -- 3 px further right. */
    assert(mixed_naive.cursor_x == 406.0);
    assert(mixed_naive.cursor_y == 299.25);
    assert(mixed_naive.cursor_x - mixed_measured.cursor_x == 3.0);

    puts("MOKO compositor geometry tests passed.");
    return 0;
}
