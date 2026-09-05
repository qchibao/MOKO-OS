#include "window_geometry.h"

static int clamp_int(int value, int minimum, int maximum)
{
    if (maximum < minimum)
        minimum = maximum;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

bool moko_rect_valid(struct moko_rect rect)
{
    return rect.width > 0 && rect.height > 0;
}

struct moko_rect moko_work_area(struct moko_rect output, struct moko_work_area_config config)
{
    if (!moko_rect_valid(output))
        return (struct moko_rect){0};

    int top = clamp_int(config.top_reserved, 0, output.height);
    int bottom = clamp_int(config.bottom_reserved, 0, output.height - top);
    const int minimum_height = clamp_int(config.minimum_height, 0, output.height);
    int available_height = output.height - top - bottom;

    if (available_height < minimum_height) {
        int deficit = minimum_height - available_height;
        const int bottom_reduction = deficit < bottom ? deficit : bottom;
        bottom -= bottom_reduction;
        deficit -= bottom_reduction;
        top -= deficit < top ? deficit : top;
        available_height = output.height - top - bottom;
    }

    return (struct moko_rect){
        .x = output.x,
        .y = output.y + top,
        .width = output.width,
        .height = available_height,
    };
}

struct moko_rect moko_centered_rect(struct moko_rect area, int width, int height, int cascade_offset)
{
    if (!moko_rect_valid(area))
        return (struct moko_rect){0};

    const int maximum_width = area.width > 80 ? area.width - 80 : area.width;
    const int maximum_height = area.height > 64 ? area.height - 64 : area.height;
    struct moko_rect result = {
        .width = clamp_int(width, 320, maximum_width),
        .height = clamp_int(height, 240, maximum_height),
    };

    result.x = area.x + (area.width - result.width) / 2 + cascade_offset;
    result.y = area.y + (area.height - result.height) / 2 + cascade_offset;

    if (result.x + result.width > area.x + area.width)
        result.x = area.x + area.width - result.width;
    if (result.y + result.height > area.y + area.height)
        result.y = area.y + area.height - result.height;
    if (result.x < area.x)
        result.x = area.x;
    if (result.y < area.y)
        result.y = area.y;
    return result;
}

struct moko_rect moko_snap_rect(struct moko_rect area, bool right)
{
    const int left_width = area.width / 2;
    struct moko_rect result = {
        .x = right ? area.x + left_width : area.x,
        .y = area.y,
        .width = right ? area.width - left_width : left_width,
        .height = area.height,
    };
    return result;
}
