/*
 * Copyright (c) 2026 MOKO OS contributors
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2014 Jari Vetoniemi
 * Copyright (c) 2023 The wlroots contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *
 * This compositor started from the wlroots 0.18 TinyWL example and replaces
 * its demonstration policy with MOKO desktop window management.
 */

#define _POSIX_C_SOURCE 200809L

#include "window_geometry.h"
#include "moko-window-control-v1-server-protocol.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#define MOKO_SHELL_APP_ID "org.moko.Shell"
#define MOKO_TOP_RESERVED 48
#define MOKO_BOTTOM_RESERVED 112
#define MOKO_SNAP_DISTANCE 24
#define MOKO_DEFAULT_POINTER_ACCELERATION 200

enum moko_cursor_mode {
    MOKO_CURSOR_PASSTHROUGH,
    MOKO_CURSOR_MOVE,
    MOKO_CURSOR_RESIZE,
};

enum moko_snap_side {
    MOKO_SNAP_NONE,
    MOKO_SNAP_LEFT,
    MOKO_SNAP_RIGHT,
};

struct moko_server;

struct moko_output {
    struct wl_list link;
    struct moko_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
};

struct moko_toplevel {
    struct wl_list link;
    struct moko_server *server;
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_minimize;
    struct wl_listener request_fullscreen;
    struct wl_listener set_title;
    struct wl_listener set_app_id;
    struct moko_rect restore_geometry;
    uint32_t window_id;
    enum moko_snap_side snap_side;
    bool mapped;
    bool is_shell;
    bool activated;
    bool minimized;
    bool maximized;
    bool fullscreen;
    bool restore_geometry_valid;
};

struct moko_popup {
    struct wlr_xdg_popup *xdg_popup;
    struct wl_listener commit;
    struct wl_listener destroy;
};

struct moko_keyboard {
    struct wl_list link;
    struct moko_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};

struct moko_pointer_device {
    struct wl_list link;
    struct moko_server *server;
    struct wlr_input_device *wlr_device;
    struct libinput_device *libinput_device;
    struct wl_listener destroy;
    bool is_touchpad;
};

struct moko_decoration {
    struct wlr_xdg_toplevel_decoration_v1 *decoration;
    struct wl_listener request_mode;
    struct wl_listener destroy;
};

struct moko_server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_scene_tree *background_tree;
    struct wlr_scene_tree *window_tree;
    struct wlr_scene_output_layout *scene_layout;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels;
    struct moko_toplevel *shell_toplevel;
    struct moko_toplevel *active_toplevel;
    uint32_t next_window_id;
    unsigned int placement_index;

    struct wlr_xdg_decoration_manager_v1 *decoration_manager;
    struct wl_listener new_decoration;

    struct wl_global *window_manager_global;
    struct wl_list window_manager_resources;

    struct wl_list pointer_devices;
    bool natural_scroll_enabled;
    int32_t pointer_acceleration;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_manager;
    struct wlr_pointer_gestures_v1 *pointer_gestures;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    struct wl_listener cursor_swipe_begin;
    struct wl_listener cursor_swipe_update;
    struct wl_listener cursor_swipe_end;
    struct wl_listener cursor_pinch_begin;
    struct wl_listener cursor_pinch_update;
    struct wl_listener cursor_pinch_end;
    struct wl_listener cursor_hold_begin;
    struct wl_listener cursor_hold_end;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_list keyboards;
    enum moko_cursor_mode cursor_mode;
    struct moko_toplevel *grabbed_toplevel;
    double grab_x;
    double grab_y;
    struct wlr_box grab_geometry;
    struct moko_rect grab_start_geometry;
    struct moko_rect grab_last_geometry;
    uint32_t resize_edges;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;
};

static const struct moko_work_area_config work_area_config = {
    .top_reserved = MOKO_TOP_RESERVED,
    .bottom_reserved = MOKO_BOTTOM_RESERVED,
    .minimum_width = 320,
    .minimum_height = 240,
};

static void report_event(const char *format, ...)
{
    char message[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    wlr_log(WLR_INFO, "%s", message);
    const char *path = getenv("MOKO_LIVE_LAUNCH_EVENTS");
    if (path == NULL || path[0] == '\0')
        path = getenv("MOKO_COMPOSITOR_EVENTS");
    if (path == NULL || path[0] == '\0')
        return;

    FILE *file = fopen(path, "a");
    if (file == NULL)
        return;
    fprintf(file, "%s\n", message);
    fclose(file);
}

struct moko_input_snapshot {
    uint32_t device_count;
    uint32_t capabilities;
    uint32_t state;
    int32_t pointer_acceleration;
};

static bool is_touchpad_device(struct libinput_device *device)
{
    return libinput_device_config_tap_get_finger_count(device) > 0
        || (libinput_device_config_scroll_get_methods(device)
            & LIBINPUT_CONFIG_SCROLL_2FG) != 0
        || libinput_device_config_dwt_is_available(device) != 0;
}

static uint32_t touchpad_capabilities(struct libinput_device *device)
{
    const int tap_fingers = libinput_device_config_tap_get_finger_count(device);
    const uint32_t scroll_methods = libinput_device_config_scroll_get_methods(device);
    const uint32_t click_methods = libinput_device_config_click_get_methods(device);
    uint32_t capabilities = MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TRACKPAD;
    if (tap_fingers > 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TAP
            | MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DRAG;
    if ((scroll_methods & LIBINPUT_CONFIG_SCROLL_2FG) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TWO_FINGER_SCROLL;
    if (libinput_device_config_scroll_has_natural_scroll(device) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_NATURAL_SCROLL;
    if (tap_fingers >= 2 || (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_SECONDARY_CLICK;
    if (libinput_device_config_accel_is_available(device) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_ACCELERATION;
    if (libinput_device_config_dwt_is_available(device) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DISABLE_WHILE_TYPING;
    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_GESTURE) != 0)
        capabilities |= MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_GESTURES;
    return capabilities;
}

static void log_config_failure(const char *setting,
                               const struct wlr_input_device *device,
                               enum libinput_config_status status)
{
    if (status == LIBINPUT_CONFIG_STATUS_SUCCESS)
        return;
    wlr_log(WLR_ERROR,
            "Could not configure %s on %s: %s",
            setting,
            device->name,
            libinput_config_status_to_str(status));
}

static void configure_touchpad(struct moko_pointer_device *pointer)
{
    struct libinput_device *device = pointer->libinput_device;
    const int tap_fingers = libinput_device_config_tap_get_finger_count(device);
    const uint32_t scroll_methods = libinput_device_config_scroll_get_methods(device);
    const uint32_t click_methods = libinput_device_config_click_get_methods(device);

    if (tap_fingers > 0) {
        log_config_failure("tap-to-click", pointer->wlr_device,
                           libinput_device_config_tap_set_enabled(
                               device, LIBINPUT_CONFIG_TAP_ENABLED));
        log_config_failure("tap button mapping", pointer->wlr_device,
                           libinput_device_config_tap_set_button_map(
                               device, LIBINPUT_CONFIG_TAP_MAP_LRM));
        log_config_failure("tap-and-drag", pointer->wlr_device,
                           libinput_device_config_tap_set_drag_enabled(
                               device, LIBINPUT_CONFIG_DRAG_ENABLED));
    }
    if ((scroll_methods & LIBINPUT_CONFIG_SCROLL_2FG) != 0) {
        log_config_failure("two-finger scrolling", pointer->wlr_device,
                           libinput_device_config_scroll_set_method(
                               device, LIBINPUT_CONFIG_SCROLL_2FG));
    }
    if (libinput_device_config_scroll_has_natural_scroll(device) != 0) {
        log_config_failure("natural scrolling", pointer->wlr_device,
                           libinput_device_config_scroll_set_natural_scroll_enabled(
                               device, pointer->server->natural_scroll_enabled));
    }
    if ((click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) != 0) {
        log_config_failure("clickfinger secondary click", pointer->wlr_device,
                           libinput_device_config_click_set_method(
                               device, LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER));
        log_config_failure("clickfinger button mapping", pointer->wlr_device,
                           libinput_device_config_click_set_clickfinger_button_map(
                               device, LIBINPUT_CONFIG_CLICKFINGER_MAP_LRM));
    }
    if (libinput_device_config_accel_is_available(device) != 0) {
        const uint32_t profiles = libinput_device_config_accel_get_profiles(device);
        if ((profiles & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE) != 0) {
            log_config_failure("adaptive acceleration", pointer->wlr_device,
                               libinput_device_config_accel_set_profile(
                                   device, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE));
        }
        log_config_failure("pointer acceleration", pointer->wlr_device,
                           libinput_device_config_accel_set_speed(
                               device, pointer->server->pointer_acceleration / 1000.0));
    }
    if (libinput_device_config_dwt_is_available(device) != 0) {
        log_config_failure("disable-while-typing", pointer->wlr_device,
                           libinput_device_config_dwt_set_enabled(
                               device, LIBINPUT_CONFIG_DWT_ENABLED));
    }
}

static struct moko_input_snapshot input_snapshot(struct moko_server *server)
{
    struct moko_input_snapshot snapshot = {
        .pointer_acceleration = server->pointer_acceleration,
    };
    uint32_t disabled_state = 0;
    double acceleration_total = 0.0;
    uint32_t acceleration_devices = 0;
    struct moko_pointer_device *pointer;
    wl_list_for_each(pointer, &server->pointer_devices, link) {
        if (!pointer->is_touchpad)
            continue;
        ++snapshot.device_count;
        struct libinput_device *device = pointer->libinput_device;
        const uint32_t capabilities = touchpad_capabilities(device);
        snapshot.capabilities |= capabilities;
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TAP) != 0
            && libinput_device_config_tap_get_enabled(device) == LIBINPUT_CONFIG_TAP_ENABLED) {
            snapshot.state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TAP_ENABLED;
        } else if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TAP) != 0) {
            disabled_state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TAP_ENABLED;
        }
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TWO_FINGER_SCROLL) != 0
            && (libinput_device_config_scroll_get_method(device)
                & LIBINPUT_CONFIG_SCROLL_2FG) != 0) {
            snapshot.state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TWO_FINGER_SCROLL_ENABLED;
        } else if ((capabilities
                    & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_TWO_FINGER_SCROLL) != 0) {
            disabled_state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_TWO_FINGER_SCROLL_ENABLED;
        }
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_NATURAL_SCROLL) != 0
            && libinput_device_config_scroll_get_natural_scroll_enabled(device) != 0) {
            snapshot.state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_NATURAL_SCROLL_ENABLED;
        } else if ((capabilities
                    & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_NATURAL_SCROLL) != 0) {
            disabled_state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_NATURAL_SCROLL_ENABLED;
        }
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_SECONDARY_CLICK) != 0) {
            const bool secondary_click =
                (libinput_device_config_click_get_method(device)
                 & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) != 0
                || (libinput_device_config_tap_get_finger_count(device) >= 2
                    && libinput_device_config_tap_get_enabled(device)
                        == LIBINPUT_CONFIG_TAP_ENABLED);
            if (secondary_click)
                snapshot.state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_SECONDARY_CLICK_ENABLED;
            else
                disabled_state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_SECONDARY_CLICK_ENABLED;
        }
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_ACCELERATION) != 0) {
            if ((libinput_device_config_accel_get_profile(device)
                 & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE) != 0) {
                snapshot.state |=
                    MOKO_WINDOW_MANAGER_V1_INPUT_STATE_ADAPTIVE_ACCELERATION_ENABLED;
            } else {
                disabled_state |=
                    MOKO_WINDOW_MANAGER_V1_INPUT_STATE_ADAPTIVE_ACCELERATION_ENABLED;
            }
            acceleration_total += libinput_device_config_accel_get_speed(device);
            ++acceleration_devices;
        }
        if ((capabilities
             & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DISABLE_WHILE_TYPING) != 0) {
            if (libinput_device_config_dwt_get_enabled(device) == LIBINPUT_CONFIG_DWT_ENABLED)
                snapshot.state |=
                    MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DISABLE_WHILE_TYPING_ENABLED;
            else
                disabled_state |=
                    MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DISABLE_WHILE_TYPING_ENABLED;
        }
        if ((capabilities & MOKO_WINDOW_MANAGER_V1_INPUT_CAPABILITY_DRAG) != 0) {
            if (libinput_device_config_tap_get_drag_enabled(device)
                == LIBINPUT_CONFIG_DRAG_ENABLED) {
                snapshot.state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DRAG_ENABLED;
            } else {
                disabled_state |= MOKO_WINDOW_MANAGER_V1_INPUT_STATE_DRAG_ENABLED;
            }
        }
    }
    snapshot.state &= ~disabled_state;
    if (acceleration_devices > 0) {
        snapshot.pointer_acceleration = (int32_t)(
            acceleration_total * 1000.0 / acceleration_devices);
    }
    return snapshot;
}

static void send_input_config_to_resource(struct moko_server *server,
                                          struct wl_resource *resource)
{
    if (wl_resource_get_version(resource) < 2)
        return;
    const struct moko_input_snapshot snapshot = input_snapshot(server);
    moko_window_manager_v1_send_input_config(resource,
                                             snapshot.device_count,
                                             snapshot.capabilities,
                                             snapshot.state,
                                             snapshot.pointer_acceleration);
}

static void broadcast_input_config(struct moko_server *server)
{
    const struct moko_input_snapshot snapshot = input_snapshot(server);
    struct wl_resource *resource;
    wl_resource_for_each(resource, &server->window_manager_resources) {
        if (wl_resource_get_version(resource) < 2)
            continue;
        moko_window_manager_v1_send_input_config(resource,
                                                 snapshot.device_count,
                                                 snapshot.capabilities,
                                                 snapshot.state,
                                                 snapshot.pointer_acceleration);
        moko_window_manager_v1_send_done(resource);
    }
    report_event("MOKO_INPUT_STATE touchpads=%u capabilities=%u state=%u acceleration=%d",
                 snapshot.device_count,
                 snapshot.capabilities,
                 snapshot.state,
                 snapshot.pointer_acceleration);
}

static const char *toplevel_app_id(const struct moko_toplevel *toplevel)
{
    const char *app_id = toplevel->xdg_toplevel->app_id;
    return app_id != NULL && app_id[0] != '\0' ? app_id : "unknown";
}

static const char *toplevel_title(const struct moko_toplevel *toplevel)
{
    const char *title = toplevel->xdg_toplevel->title;
    return title != NULL && title[0] != '\0' ? title : toplevel_app_id(toplevel);
}

static bool is_shell_app_id(const char *app_id)
{
    return app_id != NULL && strcmp(app_id, MOKO_SHELL_APP_ID) == 0;
}

static struct moko_rect output_box(struct moko_server *server, struct wlr_output *output)
{
    struct wlr_box box = {0};
    if (output == NULL)
        output = wlr_output_layout_get_center_output(server->output_layout);
    wlr_output_layout_get_box(server->output_layout, output, &box);
    return (struct moko_rect){box.x, box.y, box.width, box.height};
}

static struct wlr_output *toplevel_output(struct moko_toplevel *toplevel)
{
    struct wlr_box geometry = {0};
    wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
    const double center_x = toplevel->scene_tree->node.x + geometry.x + geometry.width / 2.0;
    const double center_y = toplevel->scene_tree->node.y + geometry.y + geometry.height / 2.0;
    struct wlr_output *output = wlr_output_layout_output_at(
        toplevel->server->output_layout, center_x, center_y);
    if (output == NULL)
        output = wlr_output_layout_get_center_output(toplevel->server->output_layout);
    return output;
}

static struct moko_rect current_geometry(struct moko_toplevel *toplevel)
{
    struct wlr_box geometry = {0};
    wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
    return (struct moko_rect){
        .x = toplevel->scene_tree->node.x + geometry.x,
        .y = toplevel->scene_tree->node.y + geometry.y,
        .width = geometry.width,
        .height = geometry.height,
    };
}

static void apply_geometry(struct moko_toplevel *toplevel, struct moko_rect geometry)
{
    if (!moko_rect_valid(geometry))
        return;
    struct wlr_box surface_geometry = {0};
    wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &surface_geometry);
    wlr_scene_node_set_position(&toplevel->scene_tree->node,
                                geometry.x - surface_geometry.x,
                                geometry.y - surface_geometry.y);
    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, geometry.width, geometry.height);
}

static uint32_t toplevel_state(const struct moko_toplevel *toplevel)
{
    uint32_t state = 0;
    if (toplevel->activated)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED;
    if (toplevel->maximized)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_MAXIMIZED;
    if (toplevel->minimized)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_MINIMIZED;
    if (toplevel->fullscreen)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_FULLSCREEN;
    if (toplevel->snap_side == MOKO_SNAP_LEFT)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_LEFT;
    if (toplevel->snap_side == MOKO_SNAP_RIGHT)
        state |= MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_RIGHT;
    return state;
}

static void send_toplevel_to_resource(struct wl_resource *resource,
                                      const struct moko_toplevel *toplevel)
{
    if (!toplevel->mapped || toplevel->is_shell)
        return;
    moko_window_manager_v1_send_window(resource,
                                       toplevel->window_id,
                                       toplevel_app_id(toplevel),
                                       toplevel_title(toplevel),
                                       toplevel_state(toplevel));
}

static void broadcast_toplevel(struct moko_toplevel *toplevel)
{
    if (!toplevel->mapped || toplevel->is_shell)
        return;
    struct wl_resource *resource;
    wl_resource_for_each(resource, &toplevel->server->window_manager_resources) {
        send_toplevel_to_resource(resource, toplevel);
        moko_window_manager_v1_send_done(resource);
    }
    report_event("MOKO_WINDOW_STATE id=%u app_id=%s state=%u title=%s",
                 toplevel->window_id,
                 toplevel_app_id(toplevel),
                 toplevel_state(toplevel),
                 toplevel_title(toplevel));
}

static struct moko_toplevel *find_toplevel(struct moko_server *server, uint32_t window_id)
{
    struct moko_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->window_id == window_id)
            return toplevel;
    }
    return NULL;
}

static void capture_restore_geometry(struct moko_toplevel *toplevel)
{
    if (toplevel->maximized || toplevel->fullscreen || toplevel->snap_side != MOKO_SNAP_NONE)
        return;
    const struct moko_rect geometry = current_geometry(toplevel);
    if (!moko_rect_valid(geometry))
        return;
    toplevel->restore_geometry = geometry;
    toplevel->restore_geometry_valid = true;
}

static struct moko_rect fallback_geometry(struct moko_toplevel *toplevel)
{
    const struct moko_rect area = moko_work_area(
        output_box(toplevel->server, toplevel_output(toplevel)), work_area_config);
    return moko_centered_rect(area, 960, 640, 0);
}

static void set_minimized(struct moko_toplevel *toplevel, bool minimized);

static void set_maximized(struct moko_toplevel *toplevel, bool maximized)
{
    if (toplevel->is_shell)
        return;
    if (maximized) {
        capture_restore_geometry(toplevel);
        if (toplevel->minimized)
            set_minimized(toplevel, false);
        toplevel->fullscreen = false;
        toplevel->snap_side = MOKO_SNAP_NONE;
        toplevel->maximized = true;
        wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
        wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel, WLR_EDGE_NONE);
        wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, true);
        apply_geometry(toplevel, moko_work_area(
            output_box(toplevel->server, toplevel_output(toplevel)), work_area_config));
    } else if (toplevel->maximized) {
        toplevel->maximized = false;
        wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
        apply_geometry(toplevel, toplevel->restore_geometry_valid
                                      ? toplevel->restore_geometry
                                      : fallback_geometry(toplevel));
        toplevel->restore_geometry_valid = false;
    } else {
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }
    broadcast_toplevel(toplevel);
}

static void set_fullscreen(struct moko_toplevel *toplevel, bool fullscreen)
{
    if (toplevel->is_shell) {
        wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
        apply_geometry(toplevel, output_box(toplevel->server, NULL));
        return;
    }
    if (fullscreen) {
        capture_restore_geometry(toplevel);
        if (toplevel->minimized)
            set_minimized(toplevel, false);
        toplevel->maximized = false;
        toplevel->snap_side = MOKO_SNAP_NONE;
        toplevel->fullscreen = true;
        wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
        wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel, WLR_EDGE_NONE);
        wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
        apply_geometry(toplevel, output_box(toplevel->server, toplevel_output(toplevel)));
    } else if (toplevel->fullscreen) {
        toplevel->fullscreen = false;
        wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
        apply_geometry(toplevel, toplevel->restore_geometry_valid
                                      ? toplevel->restore_geometry
                                      : fallback_geometry(toplevel));
        toplevel->restore_geometry_valid = false;
    } else {
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }
    broadcast_toplevel(toplevel);
}

static void set_snapped(struct moko_toplevel *toplevel, enum moko_snap_side side)
{
    if (toplevel->is_shell)
        return;
    if (side == MOKO_SNAP_NONE) {
        if (toplevel->snap_side == MOKO_SNAP_NONE)
            return;
        toplevel->snap_side = MOKO_SNAP_NONE;
        wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel, WLR_EDGE_NONE);
        apply_geometry(toplevel, toplevel->restore_geometry_valid
                                      ? toplevel->restore_geometry
                                      : fallback_geometry(toplevel));
        toplevel->restore_geometry_valid = false;
    } else {
        capture_restore_geometry(toplevel);
        if (toplevel->minimized)
            set_minimized(toplevel, false);
        toplevel->maximized = false;
        toplevel->fullscreen = false;
        toplevel->snap_side = side;
        wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
        wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
        const uint32_t tiled = WLR_EDGE_TOP | WLR_EDGE_BOTTOM
            | (side == MOKO_SNAP_LEFT ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT);
        wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel, tiled);
        const struct moko_rect area = moko_work_area(
            output_box(toplevel->server, toplevel_output(toplevel)), work_area_config);
        apply_geometry(toplevel, moko_snap_rect(area, side == MOKO_SNAP_RIGHT));
    }
    broadcast_toplevel(toplevel);
}

static void deactivate_toplevel(struct moko_toplevel *toplevel)
{
    if (toplevel == NULL || !toplevel->activated)
        return;
    toplevel->activated = false;
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
    broadcast_toplevel(toplevel);
}

static void focus_toplevel(struct moko_toplevel *toplevel)
{
    if (toplevel == NULL || !toplevel->mapped)
        return;
    struct moko_server *server = toplevel->server;
    if (toplevel->minimized)
        set_minimized(toplevel, false);

    if (server->active_toplevel != toplevel)
        deactivate_toplevel(server->active_toplevel);
    if (server->shell_toplevel != NULL && server->shell_toplevel != toplevel)
        deactivate_toplevel(server->shell_toplevel);

    if (!toplevel->is_shell) {
        wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
        wl_list_remove(&toplevel->link);
        wl_list_insert(&server->toplevels, &toplevel->link);
        server->active_toplevel = toplevel;
    } else {
        server->active_toplevel = NULL;
    }

    toplevel->activated = true;
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    if (keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(server->seat,
                                       toplevel->xdg_toplevel->base->surface,
                                       keyboard->keycodes,
                                       keyboard->num_keycodes,
                                       &keyboard->modifiers);
    }
    broadcast_toplevel(toplevel);
}

static struct moko_toplevel *first_available_toplevel(struct moko_server *server,
                                                       struct moko_toplevel *exclude)
{
    struct moko_toplevel *candidate;
    wl_list_for_each(candidate, &server->toplevels, link) {
        if (candidate != exclude && candidate->mapped && !candidate->minimized)
            return candidate;
    }
    return NULL;
}

static void focus_fallback(struct moko_server *server, struct moko_toplevel *exclude)
{
    struct moko_toplevel *candidate = first_available_toplevel(server, exclude);
    if (candidate != NULL) {
        focus_toplevel(candidate);
    } else if (server->shell_toplevel != NULL && server->shell_toplevel->mapped) {
        focus_toplevel(server->shell_toplevel);
    } else {
        server->active_toplevel = NULL;
        wlr_seat_keyboard_clear_focus(server->seat);
    }
}

static void set_minimized(struct moko_toplevel *toplevel, bool minimized)
{
    if (toplevel->is_shell)
        return;
    if (toplevel->minimized == minimized) {
        broadcast_toplevel(toplevel);
        return;
    }
    toplevel->minimized = minimized;
    wlr_scene_node_set_enabled(&toplevel->scene_tree->node, !minimized);
    if (minimized) {
        deactivate_toplevel(toplevel);
        if (toplevel->server->active_toplevel == toplevel) {
            toplevel->server->active_toplevel = NULL;
            focus_fallback(toplevel->server, toplevel);
        }
    }
    broadcast_toplevel(toplevel);
}

static void cycle_focus(struct moko_server *server, bool reverse)
{
    if (wl_list_empty(&server->toplevels))
        return;
    struct moko_toplevel *candidate = NULL;
    if (server->active_toplevel == NULL) {
        candidate = reverse
            ? wl_container_of(server->toplevels.prev, candidate, link)
            : wl_container_of(server->toplevels.next, candidate, link);
    } else {
        struct wl_list *next = reverse
            ? server->active_toplevel->link.prev
            : server->active_toplevel->link.next;
        if (next == &server->toplevels)
            next = reverse ? server->toplevels.prev : server->toplevels.next;
        candidate = wl_container_of(next, candidate, link);
    }
    if (candidate != NULL)
        focus_toplevel(candidate);
}

static void restore_for_interaction(struct moko_toplevel *toplevel)
{
    if (toplevel->fullscreen)
        set_fullscreen(toplevel, false);
    else if (toplevel->maximized)
        set_maximized(toplevel, false);
    else if (toplevel->snap_side != MOKO_SNAP_NONE)
        set_snapped(toplevel, MOKO_SNAP_NONE);
}

static void reset_cursor_mode(struct moko_server *server)
{
    if (server->grabbed_toplevel != NULL && server->cursor_mode == MOKO_CURSOR_RESIZE)
        wlr_xdg_toplevel_set_resizing(server->grabbed_toplevel->xdg_toplevel, false);
    server->cursor_mode = MOKO_CURSOR_PASSTHROUGH;
    server->grabbed_toplevel = NULL;
}

static const char *cursor_mode_name(enum moko_cursor_mode mode)
{
    return mode == MOKO_CURSOR_MOVE ? "move" : "resize";
}

static void report_interaction(struct moko_server *server,
                               const char *state,
                               enum moko_cursor_mode mode)
{
    struct moko_toplevel *toplevel = server->grabbed_toplevel;
    if (toplevel == NULL || mode == MOKO_CURSOR_PASSTHROUGH)
        return;
    const struct moko_rect geometry = strcmp(state, "begin") == 0
        ? server->grab_start_geometry : server->grab_last_geometry;
    const bool changed = geometry.x != server->grab_start_geometry.x
        || geometry.y != server->grab_start_geometry.y
        || geometry.width != server->grab_start_geometry.width
        || geometry.height != server->grab_start_geometry.height;
    report_event("MOKO_WINDOW_INTERACTION state=%s operation=%s id=%u app_id=%s "
                 "x=%d y=%d width=%d height=%d changed=%d",
                 state,
                 cursor_mode_name(mode),
                 toplevel->window_id,
                 toplevel_app_id(toplevel),
                 geometry.x,
                 geometry.y,
                 geometry.width,
                 geometry.height,
                 changed ? 1 : 0);
}

static void begin_interactive(struct moko_toplevel *toplevel,
                              enum moko_cursor_mode mode,
                              uint32_t edges)
{
    if (toplevel == NULL || toplevel->is_shell || toplevel->minimized)
        return;
    struct moko_server *server = toplevel->server;
    struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
    if (focused_surface == NULL
        || toplevel->xdg_toplevel->base->surface
            != wlr_surface_get_root_surface(focused_surface)) {
        return;
    }

    restore_for_interaction(toplevel);
    server->grabbed_toplevel = toplevel;
    server->cursor_mode = mode;
    server->grab_start_geometry = current_geometry(toplevel);
    server->grab_last_geometry = server->grab_start_geometry;
    if (mode == MOKO_CURSOR_MOVE) {
        server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
        server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
    } else {
        struct wlr_box geometry = {0};
        wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
        const double border_x = toplevel->scene_tree->node.x + geometry.x
            + ((edges & WLR_EDGE_RIGHT) ? geometry.width : 0);
        const double border_y = toplevel->scene_tree->node.y + geometry.y
            + ((edges & WLR_EDGE_BOTTOM) ? geometry.height : 0);
        server->grab_x = server->cursor->x - border_x;
        server->grab_y = server->cursor->y - border_y;
        server->grab_geometry = geometry;
        server->grab_geometry.x += toplevel->scene_tree->node.x;
        server->grab_geometry.y += toplevel->scene_tree->node.y;
        server->resize_edges = edges;
        wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, true);
    }
    report_interaction(server, "begin", mode);
}

static void process_cursor_move(struct moko_server *server)
{
    struct moko_toplevel *toplevel = server->grabbed_toplevel;
    const int x = (int)(server->cursor->x - server->grab_x);
    const int y = (int)(server->cursor->y - server->grab_y);
    wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
    server->grab_last_geometry = current_geometry(toplevel);
}

static void process_cursor_resize(struct moko_server *server)
{
    struct moko_toplevel *toplevel = server->grabbed_toplevel;
    const double border_x = server->cursor->x - server->grab_x;
    const double border_y = server->cursor->y - server->grab_y;
    int left = server->grab_geometry.x;
    int right = server->grab_geometry.x + server->grab_geometry.width;
    int top = server->grab_geometry.y;
    int bottom = server->grab_geometry.y + server->grab_geometry.height;

    if (server->resize_edges & WLR_EDGE_TOP)
        top = (int)border_y;
    else if (server->resize_edges & WLR_EDGE_BOTTOM)
        bottom = (int)border_y;
    if (server->resize_edges & WLR_EDGE_LEFT)
        left = (int)border_x;
    else if (server->resize_edges & WLR_EDGE_RIGHT)
        right = (int)border_x;

    const int min_width = toplevel->xdg_toplevel->current.min_width > 0
        ? toplevel->xdg_toplevel->current.min_width : 320;
    const int min_height = toplevel->xdg_toplevel->current.min_height > 0
        ? toplevel->xdg_toplevel->current.min_height : 240;
    if (right - left < min_width) {
        if (server->resize_edges & WLR_EDGE_LEFT)
            left = right - min_width;
        else
            right = left + min_width;
    }
    if (bottom - top < min_height) {
        if (server->resize_edges & WLR_EDGE_TOP)
            top = bottom - min_height;
        else
            bottom = top + min_height;
    }
    server->grab_last_geometry = (struct moko_rect){left, top, right - left, bottom - top};
    apply_geometry(toplevel, server->grab_last_geometry);
}

static struct moko_toplevel *toplevel_at(struct moko_server *server,
                                         double layout_x,
                                         double layout_y,
                                         struct wlr_surface **surface,
                                         double *surface_x,
                                         double *surface_y)
{
    struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node,
                                                    layout_x,
                                                    layout_y,
                                                    surface_x,
                                                    surface_y);
    if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;
    struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(
        wlr_scene_buffer_from_node(node));
    if (scene_surface == NULL)
        return NULL;
    *surface = scene_surface->surface;
    struct wlr_scene_tree *tree = node->parent;
    while (tree != NULL && tree->node.data == NULL)
        tree = tree->node.parent;
    return tree != NULL ? tree->node.data : NULL;
}

static void process_cursor_motion(struct moko_server *server, uint32_t time_msec)
{
    if (server->cursor_mode == MOKO_CURSOR_MOVE) {
        process_cursor_move(server);
        return;
    }
    if (server->cursor_mode == MOKO_CURSOR_RESIZE) {
        process_cursor_resize(server);
        return;
    }

    double surface_x = 0;
    double surface_y = 0;
    struct wlr_surface *surface = NULL;
    struct moko_toplevel *toplevel = toplevel_at(server,
                                                 server->cursor->x,
                                                 server->cursor->y,
                                                 &surface,
                                                 &surface_x,
                                                 &surface_y);
    if (toplevel == NULL)
        wlr_cursor_set_xcursor(server->cursor, server->cursor_manager, "default");
    if (surface != NULL) {
        wlr_seat_pointer_notify_enter(server->seat, surface, surface_x, surface_y);
        wlr_seat_pointer_notify_motion(server->seat, time_msec, surface_x, surface_y);
    } else {
        wlr_seat_pointer_clear_focus(server->seat);
    }
}

static void cursor_motion(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

static void finish_move_with_optional_snap(struct moko_server *server)
{
    struct moko_toplevel *toplevel = server->grabbed_toplevel;
    if (toplevel == NULL)
        return;
    const struct moko_rect area = moko_work_area(
        output_box(server, toplevel_output(toplevel)), work_area_config);
    if (server->cursor->x <= area.x + MOKO_SNAP_DISTANCE)
        set_snapped(toplevel, MOKO_SNAP_LEFT);
    else if (server->cursor->x >= area.x + area.width - MOKO_SNAP_DISTANCE)
        set_snapped(toplevel, MOKO_SNAP_RIGHT);
    else
        capture_restore_geometry(toplevel);
}

static uint32_t active_modifiers(struct moko_server *server)
{
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    return keyboard != NULL ? wlr_keyboard_get_modifiers(keyboard) : 0;
}

static void cursor_button(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;
    const enum moko_cursor_mode previous_mode = server->cursor_mode;

    wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
    double surface_x = 0;
    double surface_y = 0;
    struct wlr_surface *surface = NULL;
    struct moko_toplevel *toplevel = toplevel_at(server,
                                                 server->cursor->x,
                                                 server->cursor->y,
                                                 &surface,
                                                 &surface_x,
                                                 &surface_y);
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (previous_mode == MOKO_CURSOR_MOVE)
            finish_move_with_optional_snap(server);
        report_interaction(server, "end", previous_mode);
        reset_cursor_mode(server);
        return;
    }

    focus_toplevel(toplevel);
    const uint32_t modifiers = active_modifiers(server);
    if ((modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO)) == 0)
        return;
    if (event->button == BTN_LEFT)
        begin_interactive(toplevel, MOKO_CURSOR_MOVE, WLR_EDGE_NONE);
    else if (event->button == BTN_RIGHT)
        begin_interactive(toplevel, MOKO_CURSOR_RESIZE, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
}

static void cursor_axis(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(server->seat,
                                 event->time_msec,
                                 event->orientation,
                                 event->delta,
                                 event->delta_discrete,
                                 event->source,
                                 event->relative_direction);
}

static void cursor_frame(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

static void cursor_swipe_begin(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_swipe_begin);
    struct wlr_pointer_swipe_begin_event *event = data;
    wlr_pointer_gestures_v1_send_swipe_begin(
        server->pointer_gestures, server->seat, event->time_msec, event->fingers);
}

static void cursor_swipe_update(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_swipe_update);
    struct wlr_pointer_swipe_update_event *event = data;
    wlr_pointer_gestures_v1_send_swipe_update(
        server->pointer_gestures, server->seat, event->time_msec, event->dx, event->dy);
}

static void cursor_swipe_end(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_swipe_end);
    struct wlr_pointer_swipe_end_event *event = data;
    wlr_pointer_gestures_v1_send_swipe_end(
        server->pointer_gestures, server->seat, event->time_msec, event->cancelled);
}

static void cursor_pinch_begin(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_pinch_begin);
    struct wlr_pointer_pinch_begin_event *event = data;
    wlr_pointer_gestures_v1_send_pinch_begin(
        server->pointer_gestures, server->seat, event->time_msec, event->fingers);
}

static void cursor_pinch_update(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_pinch_update);
    struct wlr_pointer_pinch_update_event *event = data;
    wlr_pointer_gestures_v1_send_pinch_update(server->pointer_gestures,
                                              server->seat,
                                              event->time_msec,
                                              event->dx,
                                              event->dy,
                                              event->scale,
                                              event->rotation);
}

static void cursor_pinch_end(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_pinch_end);
    struct wlr_pointer_pinch_end_event *event = data;
    wlr_pointer_gestures_v1_send_pinch_end(
        server->pointer_gestures, server->seat, event->time_msec, event->cancelled);
}

static void cursor_hold_begin(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_hold_begin);
    struct wlr_pointer_hold_begin_event *event = data;
    wlr_pointer_gestures_v1_send_hold_begin(
        server->pointer_gestures, server->seat, event->time_msec, event->fingers);
}

static void cursor_hold_end(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, cursor_hold_end);
    struct wlr_pointer_hold_end_event *event = data;
    wlr_pointer_gestures_v1_send_hold_end(
        server->pointer_gestures, server->seat, event->time_msec, event->cancelled);
}

static void request_brightness_step(struct moko_server *server, int32_t delta)
{
    struct wl_resource *resource;
    wl_resource_for_each(resource, &server->window_manager_resources)
        moko_window_manager_v1_send_brightness_step(resource, delta);
    report_event("MOKO_SYSTEM_KEY action=brightness delta=%d", delta);
}

static bool handle_keybinding(struct moko_server *server,
                              xkb_keysym_t symbol,
                              uint32_t modifiers)
{
    const bool alt_or_logo = modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO);
    const bool logo = modifiers & WLR_MODIFIER_LOGO;
    if (symbol == XKB_KEY_XF86MonBrightnessUp) {
        request_brightness_step(server, 5);
        return true;
    }
    if (symbol == XKB_KEY_XF86MonBrightnessDown) {
        request_brightness_step(server, -5);
        return true;
    }
    if (alt_or_logo && symbol == XKB_KEY_Tab) {
        cycle_focus(server, modifiers & WLR_MODIFIER_SHIFT);
        return true;
    }
    if ((modifiers & WLR_MODIFIER_ALT) && symbol == XKB_KEY_F4) {
        if (server->active_toplevel != NULL)
            wlr_xdg_toplevel_send_close(server->active_toplevel->xdg_toplevel);
        return true;
    }
    if (!logo || server->active_toplevel == NULL)
        return false;
    switch (symbol) {
    case XKB_KEY_Left:
        set_snapped(server->active_toplevel, MOKO_SNAP_LEFT);
        return true;
    case XKB_KEY_Right:
        set_snapped(server->active_toplevel, MOKO_SNAP_RIGHT);
        return true;
    case XKB_KEY_Up:
        set_maximized(server->active_toplevel, true);
        return true;
    case XKB_KEY_Down:
        if (server->active_toplevel->fullscreen)
            set_fullscreen(server->active_toplevel, false);
        else if (server->active_toplevel->maximized)
            set_maximized(server->active_toplevel, false);
        else if (server->active_toplevel->snap_side != MOKO_SNAP_NONE)
            set_snapped(server->active_toplevel, MOKO_SNAP_NONE);
        else
            set_minimized(server->active_toplevel, true);
        return true;
    case XKB_KEY_f:
    case XKB_KEY_F:
        set_fullscreen(server->active_toplevel, !server->active_toplevel->fullscreen);
        return true;
    default:
        return false;
    }
}

static void keyboard_modifiers(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
                                       &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_key(struct wl_listener *listener, void *data)
{
    struct moko_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    const uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *symbols = NULL;
    const int symbol_count = xkb_state_key_get_syms(
        keyboard->wlr_keyboard->xkb_state, keycode, &symbols);
    const uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
    bool handled = false;
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int index = 0; index < symbol_count; ++index)
            handled = handle_keybinding(keyboard->server, symbols[index], modifiers) || handled;
    }
    if (!handled) {
        wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(keyboard->server->seat,
                                     event->time_msec,
                                     event->keycode,
                                     event->state);
    }
}

static void keyboard_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void add_keyboard(struct moko_server *server, struct wlr_input_device *device)
{
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
    struct moko_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    if (keyboard == NULL)
        return;
    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard;

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(
        context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    keyboard->modifiers.notify = keyboard_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = keyboard_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
    keyboard->destroy.notify = keyboard_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);
    wl_list_insert(&server->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(server->seat, wlr_keyboard);
}

static void pointer_device_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_pointer_device *pointer = wl_container_of(listener, pointer, destroy);
    struct moko_server *server = pointer->server;
    wl_list_remove(&pointer->destroy.link);
    wl_list_remove(&pointer->link);
    free(pointer);
    broadcast_input_config(server);
}

static void add_pointer_device(struct moko_server *server, struct wlr_input_device *device)
{
    wlr_cursor_attach_input_device(server->cursor, device);
    if (!wlr_input_device_is_libinput(device))
        return;

    struct moko_pointer_device *pointer = calloc(1, sizeof(*pointer));
    if (pointer == NULL)
        return;
    pointer->server = server;
    pointer->wlr_device = device;
    pointer->libinput_device = wlr_libinput_get_device_handle(device);
    pointer->is_touchpad = is_touchpad_device(pointer->libinput_device);
    pointer->destroy.notify = pointer_device_destroy;
    wl_signal_add(&device->events.destroy, &pointer->destroy);
    wl_list_insert(&server->pointer_devices, &pointer->link);

    if (pointer->is_touchpad) {
        configure_touchpad(pointer);
        report_event("MOKO_INPUT_DEVICE state=configured name=%s capabilities=%u",
                     device->name,
                     touchpad_capabilities(pointer->libinput_device));
    }
    broadcast_input_config(server);
}

static void new_input(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        add_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        add_pointer_device(server, device);
        break;
    default:
        break;
    }
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards))
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server->seat, capabilities);
}

static void request_cursor(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    if (server->seat->pointer_state.focused_client == event->seat_client) {
        wlr_cursor_set_surface(server->cursor,
                               event->surface,
                               event->hotspot_x,
                               event->hotspot_y);
    }
}

static void request_set_selection(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void output_frame(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_output *output = wl_container_of(listener, output, frame);
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
        output->server->scene, output->wlr_output);
    wlr_scene_output_commit(scene_output, NULL);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data)
{
    struct moko_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void position_shell(struct moko_server *server)
{
    if (server->shell_toplevel == NULL || !server->shell_toplevel->mapped)
        return;
    server->shell_toplevel->fullscreen = true;
    wlr_xdg_toplevel_set_fullscreen(server->shell_toplevel->xdg_toplevel, true);
    apply_geometry(server->shell_toplevel, output_box(server, NULL));
    wlr_scene_node_lower_to_bottom(&server->shell_toplevel->scene_tree->node);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_output *output = wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

static void new_output(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;
    if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
        wlr_log(WLR_ERROR, "Failed to initialize output renderer");
        return;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL)
        wlr_output_state_set_mode(&state, mode);
    if (!wlr_output_commit_state(wlr_output, &state))
        wlr_log(WLR_ERROR, "Failed to enable output %s", wlr_output->name);
    wlr_output_state_finish(&state);

    struct moko_output *output = calloc(1, sizeof(*output));
    if (output == NULL)
        return;
    output->server = server;
    output->wlr_output = wlr_output;
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *layout_output = wlr_output_layout_add_auto(
        server->output_layout, wlr_output);
    struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
    position_shell(server);
}

static void classify_toplevel(struct moko_toplevel *toplevel)
{
    const bool should_be_shell = is_shell_app_id(toplevel->xdg_toplevel->app_id);
    if (should_be_shell == toplevel->is_shell)
        return;
    toplevel->is_shell = should_be_shell;
    if (should_be_shell) {
        wlr_scene_node_reparent(&toplevel->scene_tree->node, toplevel->server->background_tree);
        wlr_scene_node_lower_to_bottom(&toplevel->scene_tree->node);
        toplevel->server->shell_toplevel = toplevel;
    } else {
        wlr_scene_node_reparent(&toplevel->scene_tree->node, toplevel->server->window_tree);
    }
}

static void toplevel_map(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, map);
    classify_toplevel(toplevel);
    toplevel->mapped = true;
    if (toplevel->is_shell) {
        position_shell(toplevel->server);
        if (toplevel->server->active_toplevel == NULL)
            focus_toplevel(toplevel);
        const struct moko_rect geometry = current_geometry(toplevel);
        report_event("MOKO_COMPOSITOR_SHELL state=mapped app_id=%s width=%d height=%d fullscreen=%d",
                     toplevel_app_id(toplevel),
                     geometry.width,
                     geometry.height,
                     toplevel->fullscreen);
        return;
    }

    wl_list_insert(&toplevel->server->toplevels, &toplevel->link);
    struct wlr_box initial = {0};
    wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &initial);
    const struct moko_rect area = moko_work_area(
        output_box(toplevel->server, NULL), work_area_config);
    const int width = initial.width > 0 ? initial.width : 960;
    const int height = initial.height > 0 ? initial.height : 640;
    const int cascade = (int)(toplevel->server->placement_index++ % 6) * 28;
    apply_geometry(toplevel, moko_centered_rect(area, width, height, cascade));
    capture_restore_geometry(toplevel);

    if (toplevel->xdg_toplevel->requested.fullscreen)
        set_fullscreen(toplevel, true);
    else if (toplevel->xdg_toplevel->requested.maximized)
        set_maximized(toplevel, true);
    else if (toplevel->xdg_toplevel->requested.minimized)
        set_minimized(toplevel, true);

    focus_toplevel(toplevel);
    broadcast_toplevel(toplevel);
    report_event("MOKO_COMPOSITOR_WINDOW state=mapped id=%u app_id=%s",
                 toplevel->window_id,
                 toplevel_app_id(toplevel));
}

static void toplevel_unmap(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
    if (toplevel == toplevel->server->grabbed_toplevel)
        reset_cursor_mode(toplevel->server);
    if (!toplevel->mapped)
        return;
    toplevel->mapped = false;
    if (toplevel->is_shell) {
        if (toplevel->server->shell_toplevel == toplevel)
            toplevel->server->shell_toplevel = NULL;
    } else {
        struct wl_resource *resource;
        wl_resource_for_each(resource, &toplevel->server->window_manager_resources) {
            moko_window_manager_v1_send_window_removed(resource, toplevel->window_id);
            moko_window_manager_v1_send_done(resource);
        }
        wl_list_remove(&toplevel->link);
        wl_list_init(&toplevel->link);
        report_event("MOKO_COMPOSITOR_WINDOW state=unmapped id=%u app_id=%s",
                     toplevel->window_id,
                     toplevel_app_id(toplevel));
    }
    if (toplevel->server->active_toplevel == toplevel) {
        toplevel->server->active_toplevel = NULL;
        focus_fallback(toplevel->server, toplevel);
    }
}

static void toplevel_commit(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
    classify_toplevel(toplevel);
    if (toplevel->xdg_toplevel->base->initial_commit) {
        const uint32_t capabilities = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE
            | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN
            | WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
        if (toplevel->xdg_toplevel->base->client->shell->version
            >= XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
            wlr_xdg_toplevel_set_wm_capabilities(toplevel->xdg_toplevel, capabilities);
        }
        if (toplevel->is_shell) {
            const struct moko_rect geometry = output_box(toplevel->server, NULL);
            toplevel->fullscreen = true;
            wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
            if (moko_rect_valid(geometry))
                wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
                                          geometry.width,
                                          geometry.height);
        } else {
            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
        }
    }
}

static void toplevel_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->request_maximize.link);
    wl_list_remove(&toplevel->request_minimize.link);
    wl_list_remove(&toplevel->request_fullscreen.link);
    wl_list_remove(&toplevel->set_title.link);
    wl_list_remove(&toplevel->set_app_id.link);
    free(toplevel);
}

static void request_move(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
    begin_interactive(toplevel, MOKO_CURSOR_MOVE, WLR_EDGE_NONE);
}

static void request_resize(struct wl_listener *listener, void *data)
{
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
    const struct wlr_xdg_toplevel_resize_event *event = data;
    begin_interactive(toplevel, MOKO_CURSOR_RESIZE, event->edges);
}

static void request_maximize(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
    if (!toplevel->xdg_toplevel->base->initialized)
        return;
    set_maximized(toplevel, toplevel->xdg_toplevel->requested.maximized);
}

static void request_minimize(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, request_minimize);
    if (!toplevel->xdg_toplevel->base->initialized)
        return;
    set_minimized(toplevel, true);
}

static void request_fullscreen(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
    if (!toplevel->xdg_toplevel->base->initialized)
        return;
    set_fullscreen(toplevel, toplevel->xdg_toplevel->requested.fullscreen);
}

static void set_title(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, set_title);
    broadcast_toplevel(toplevel);
}

static void set_app_id(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_toplevel *toplevel = wl_container_of(listener, toplevel, set_app_id);
    classify_toplevel(toplevel);
    broadcast_toplevel(toplevel);
}

static void new_xdg_toplevel(struct wl_listener *listener, void *data)
{
    struct moko_server *server = wl_container_of(listener, server, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;
    struct moko_toplevel *toplevel = calloc(1, sizeof(*toplevel));
    if (toplevel == NULL) {
        wl_client_post_no_memory(wl_resource_get_client(xdg_toplevel->resource));
        return;
    }
    toplevel->server = server;
    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->window_id = ++server->next_window_id;
    wl_list_init(&toplevel->link);
    toplevel->scene_tree = wlr_scene_xdg_surface_create(server->window_tree,
                                                        xdg_toplevel->base);
    toplevel->scene_tree->node.data = toplevel;
    xdg_toplevel->base->data = toplevel->scene_tree;

    toplevel->map.notify = toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
    toplevel->unmap.notify = toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
    toplevel->commit.notify = toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);
    toplevel->destroy.notify = toplevel_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);
    toplevel->request_move.notify = request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
    toplevel->request_resize.notify = request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
    toplevel->request_maximize.notify = request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
    toplevel->request_minimize.notify = request_minimize;
    wl_signal_add(&xdg_toplevel->events.request_minimize, &toplevel->request_minimize);
    toplevel->request_fullscreen.notify = request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
    toplevel->set_title.notify = set_title;
    wl_signal_add(&xdg_toplevel->events.set_title, &toplevel->set_title);
    toplevel->set_app_id.notify = set_app_id;
    wl_signal_add(&xdg_toplevel->events.set_app_id, &toplevel->set_app_id);
}

static void popup_commit(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_popup *popup = wl_container_of(listener, popup, commit);
    if (popup->xdg_popup->base->initial_commit)
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
}

static void popup_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_popup *popup = wl_container_of(listener, popup, destroy);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);
    free(popup);
}

static void new_xdg_popup(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_xdg_popup *xdg_popup = data;
    struct moko_popup *popup = calloc(1, sizeof(*popup));
    if (popup == NULL)
        return;
    popup->xdg_popup = xdg_popup;
    struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    assert(parent != NULL);
    struct wlr_scene_tree *parent_tree = parent->data;
    xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
    popup->commit.notify = popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);
    popup->destroy.notify = popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

static void decoration_request_mode(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_decoration *decoration = wl_container_of(listener, decoration, request_mode);
    wlr_xdg_toplevel_decoration_v1_set_mode(
        decoration->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void decoration_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    struct moko_decoration *decoration = wl_container_of(listener, decoration, destroy);
    wl_list_remove(&decoration->request_mode.link);
    wl_list_remove(&decoration->destroy.link);
    free(decoration);
}

static void new_decoration(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_xdg_toplevel_decoration_v1 *xdg_decoration = data;
    struct moko_decoration *decoration = calloc(1, sizeof(*decoration));
    if (decoration == NULL)
        return;
    decoration->decoration = xdg_decoration;
    decoration->request_mode.notify = decoration_request_mode;
    wl_signal_add(&xdg_decoration->events.request_mode, &decoration->request_mode);
    decoration->destroy.notify = decoration_destroy;
    wl_signal_add(&xdg_decoration->events.destroy, &decoration->destroy);
    wlr_xdg_toplevel_decoration_v1_set_mode(
        xdg_decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void manager_destroy(struct wl_client *client, struct wl_resource *resource)
{
    (void)client;
    wl_resource_destroy(resource);
}

static void manager_activate(struct wl_client *client,
                             struct wl_resource *resource,
                             uint32_t window_id)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    focus_toplevel(find_toplevel(server, window_id));
}

static void manager_set_minimized(struct wl_client *client,
                                  struct wl_resource *resource,
                                  uint32_t window_id,
                                  uint32_t minimized)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    struct moko_toplevel *toplevel = find_toplevel(server, window_id);
    if (toplevel != NULL)
        set_minimized(toplevel, minimized != 0);
}

static void manager_set_maximized(struct wl_client *client,
                                  struct wl_resource *resource,
                                  uint32_t window_id,
                                  uint32_t maximized)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    struct moko_toplevel *toplevel = find_toplevel(server, window_id);
    if (toplevel != NULL)
        set_maximized(toplevel, maximized != 0);
}

static void manager_set_fullscreen(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t window_id,
                                   uint32_t fullscreen)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    struct moko_toplevel *toplevel = find_toplevel(server, window_id);
    if (toplevel != NULL)
        set_fullscreen(toplevel, fullscreen != 0);
}

static void manager_snap(struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t window_id,
                         uint32_t side)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    struct moko_toplevel *toplevel = find_toplevel(server, window_id);
    if (toplevel == NULL)
        return;
    if (side == MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_LEFT)
        set_snapped(toplevel, MOKO_SNAP_LEFT);
    else if (side == MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_RIGHT)
        set_snapped(toplevel, MOKO_SNAP_RIGHT);
    else
        set_snapped(toplevel, MOKO_SNAP_NONE);
}

static void manager_close(struct wl_client *client,
                          struct wl_resource *resource,
                          uint32_t window_id)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    struct moko_toplevel *toplevel = find_toplevel(server, window_id);
    if (toplevel != NULL)
        wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
}

static void manager_set_natural_scroll(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t enabled)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    server->natural_scroll_enabled = enabled != 0;
    struct moko_pointer_device *pointer;
    wl_list_for_each(pointer, &server->pointer_devices, link) {
        if (!pointer->is_touchpad
            || libinput_device_config_scroll_has_natural_scroll(
                   pointer->libinput_device) == 0) {
            continue;
        }
        log_config_failure("natural scrolling", pointer->wlr_device,
                           libinput_device_config_scroll_set_natural_scroll_enabled(
                               pointer->libinput_device,
                               server->natural_scroll_enabled));
    }
    broadcast_input_config(server);
}

static void manager_set_pointer_acceleration(struct wl_client *client,
                                             struct wl_resource *resource,
                                             int32_t speed)
{
    (void)client;
    struct moko_server *server = wl_resource_get_user_data(resource);
    if (speed < -1000)
        speed = -1000;
    if (speed > 1000)
        speed = 1000;
    server->pointer_acceleration = speed;
    struct moko_pointer_device *pointer;
    wl_list_for_each(pointer, &server->pointer_devices, link) {
        if (!pointer->is_touchpad
            || libinput_device_config_accel_is_available(pointer->libinput_device) == 0) {
            continue;
        }
        log_config_failure("pointer acceleration", pointer->wlr_device,
                           libinput_device_config_accel_set_speed(
                               pointer->libinput_device, speed / 1000.0));
    }
    broadcast_input_config(server);
}

static const struct moko_window_manager_v1_interface window_manager_implementation = {
    .destroy = manager_destroy,
    .activate = manager_activate,
    .set_minimized = manager_set_minimized,
    .set_maximized = manager_set_maximized,
    .set_fullscreen = manager_set_fullscreen,
    .snap = manager_snap,
    .close = manager_close,
    .set_natural_scroll = manager_set_natural_scroll,
    .set_pointer_acceleration = manager_set_pointer_acceleration,
};

static void manager_resource_destroy(struct wl_resource *resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

static void bind_window_manager(struct wl_client *client,
                                void *data,
                                uint32_t version,
                                uint32_t id)
{
    struct moko_server *server = data;
    struct wl_resource *resource = wl_resource_create(
        client, &moko_window_manager_v1_interface, version, id);
    if (resource == NULL) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource,
                                   &window_manager_implementation,
                                   server,
                                   manager_resource_destroy);
    wl_list_insert(&server->window_manager_resources, wl_resource_get_link(resource));
    struct moko_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link)
        send_toplevel_to_resource(resource, toplevel);
    send_input_config_to_resource(server, resource);
    moko_window_manager_v1_send_done(resource);
}

static int terminate_signal(int signal_number, void *data)
{
    (void)signal_number;
    wl_display_terminate(data);
    return 0;
}

static void usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s [--socket NAME] [--debug]\n", program);
}

int main(int argc, char **argv)
{
    const char *socket_name = NULL;
    bool debug = false;
    static const struct option options[] = {
        {"socket", required_argument, NULL, 's'},
        {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    int option;
    while ((option = getopt_long(argc, argv, "s:dh", options, NULL)) != -1) {
        switch (option) {
        case 's':
            socket_name = optarg;
            break;
        case 'd':
            debug = true;
            break;
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 2;
        }
    }
    if (optind != argc) {
        usage(stderr, argv[0]);
        return 2;
    }

    wlr_log_init(debug ? WLR_DEBUG : WLR_INFO, NULL);
    struct moko_server server = {0};
    server.next_window_id = 100;
    server.natural_scroll_enabled = true;
    server.pointer_acceleration = MOKO_DEFAULT_POINTER_ACCELERATION;
    wl_list_init(&server.outputs);
    wl_list_init(&server.toplevels);
    wl_list_init(&server.keyboards);
    wl_list_init(&server.pointer_devices);
    wl_list_init(&server.window_manager_resources);

    server.display = wl_display_create();
    if (server.display == NULL)
        return 1;
    struct wl_event_loop *event_loop = wl_display_get_event_loop(server.display);
    server.backend = wlr_backend_autocreate(event_loop, NULL);
    if (server.backend == NULL) {
        wlr_log(WLR_ERROR, "Failed to create wlroots backend");
        wl_display_destroy(server.display);
        return 1;
    }
    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL) {
        wlr_log(WLR_ERROR, "Failed to create renderer");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }
    wlr_renderer_init_wl_display(server.renderer, server.display);
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (server.allocator == NULL) {
        wlr_log(WLR_ERROR, "Failed to create allocator");
        wlr_renderer_destroy(server.renderer);
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.display);
        return 1;
    }

    wlr_compositor_create(server.display, 5, server.renderer);
    wlr_subcompositor_create(server.display);
    wlr_data_device_manager_create(server.display);

    server.output_layout = wlr_output_layout_create(server.display);
    server.new_output.notify = new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);
    server.scene = wlr_scene_create();
    server.background_tree = wlr_scene_tree_create(&server.scene->tree);
    server.window_tree = wlr_scene_tree_create(&server.scene->tree);
    server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

    server.xdg_shell = wlr_xdg_shell_create(server.display, 6);
    server.new_xdg_toplevel.notify = new_xdg_toplevel;
    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
    server.new_xdg_popup.notify = new_xdg_popup;
    wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

    server.decoration_manager = wlr_xdg_decoration_manager_v1_create(server.display);
    server.new_decoration.notify = new_decoration;
    wl_signal_add(&server.decoration_manager->events.new_toplevel_decoration,
                  &server.new_decoration);

    server.window_manager_global = wl_global_create(server.display,
                                                    &moko_window_manager_v1_interface,
                                                    2,
                                                    &server,
                                                    bind_window_manager);
    if (server.window_manager_global == NULL) {
        wlr_log(WLR_ERROR, "Failed to create MOKO window manager protocol");
        return 1;
    }

    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    server.cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    server.pointer_gestures = wlr_pointer_gestures_v1_create(server.display);
    if (server.pointer_gestures == NULL) {
        wlr_log(WLR_ERROR, "Failed to create pointer gesture protocol");
        return 1;
    }
    server.cursor_mode = MOKO_CURSOR_PASSTHROUGH;
    server.cursor_motion.notify = cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
    server.cursor_button.notify = cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);
    server.cursor_axis.notify = cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
    server.cursor_frame.notify = cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);
    server.cursor_swipe_begin.notify = cursor_swipe_begin;
    wl_signal_add(&server.cursor->events.swipe_begin, &server.cursor_swipe_begin);
    server.cursor_swipe_update.notify = cursor_swipe_update;
    wl_signal_add(&server.cursor->events.swipe_update, &server.cursor_swipe_update);
    server.cursor_swipe_end.notify = cursor_swipe_end;
    wl_signal_add(&server.cursor->events.swipe_end, &server.cursor_swipe_end);
    server.cursor_pinch_begin.notify = cursor_pinch_begin;
    wl_signal_add(&server.cursor->events.pinch_begin, &server.cursor_pinch_begin);
    server.cursor_pinch_update.notify = cursor_pinch_update;
    wl_signal_add(&server.cursor->events.pinch_update, &server.cursor_pinch_update);
    server.cursor_pinch_end.notify = cursor_pinch_end;
    wl_signal_add(&server.cursor->events.pinch_end, &server.cursor_pinch_end);
    server.cursor_hold_begin.notify = cursor_hold_begin;
    wl_signal_add(&server.cursor->events.hold_begin, &server.cursor_hold_begin);
    server.cursor_hold_end.notify = cursor_hold_end;
    wl_signal_add(&server.cursor->events.hold_end, &server.cursor_hold_end);

    server.seat = wlr_seat_create(server.display, "seat0");
    server.new_input.notify = new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.request_cursor.notify = request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
    server.request_set_selection.notify = request_set_selection;
    wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

    const int socket_result = socket_name != NULL
        ? wl_display_add_socket(server.display, socket_name)
        : 0;
    if ((socket_name != NULL && socket_result != 0)
        || (socket_name == NULL && (socket_name = wl_display_add_socket_auto(server.display)) == NULL)) {
        wlr_log(WLR_ERROR, "Failed to create Wayland socket: %s", strerror(errno));
        return 1;
    }

    wl_event_loop_add_signal(event_loop, SIGTERM, terminate_signal, server.display);
    wl_event_loop_add_signal(event_loop, SIGINT, terminate_signal, server.display);
    if (!wlr_backend_start(server.backend)) {
        wlr_log(WLR_ERROR, "Failed to start wlroots backend");
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket_name, true);
    report_event("MOKO_COMPOSITOR_READY socket=%s pid=%ld", socket_name, (long)getpid());
    wl_display_run(server.display);

    wl_display_destroy_clients(server.display);
    wlr_scene_node_destroy(&server.scene->tree.node);
    wlr_xcursor_manager_destroy(server.cursor_manager);
    wlr_cursor_destroy(server.cursor);
    wlr_allocator_destroy(server.allocator);
    wlr_renderer_destroy(server.renderer);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);
    return 0;
}
