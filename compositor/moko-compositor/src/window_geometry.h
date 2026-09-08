#pragma once

#include <stdbool.h>

struct moko_rect {
    int x;
    int y;
    int width;
    int height;
};

struct moko_work_area_config {
    int top_reserved;
    int bottom_reserved;
    int minimum_width;
    int minimum_height;
};

bool moko_rect_valid(struct moko_rect rect);
struct moko_rect moko_work_area(struct moko_rect output, struct moko_work_area_config config);
struct moko_rect moko_centered_rect(struct moko_rect area, int width, int height, int cascade_offset);
struct moko_rect moko_snap_rect(struct moko_rect area, bool right);
struct moko_rect moko_translate_rect(struct moko_rect rect, double dx, double dy);
