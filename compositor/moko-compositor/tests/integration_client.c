#define _GNU_SOURCE

#include "moko-window-control-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define TEST_WIDTH 320
#define TEST_HEIGHT 240
#define TEST_STRIDE (TEST_WIDTH * 4)
#define TEST_BUFFER_SIZE (TEST_STRIDE * TEST_HEIGHT)

struct observed_window {
    uint32_t id;
    uint32_t state;
    char app_id[96];
    bool present;
};

struct test_state;

struct test_window {
    struct test_state *state;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_buffer *buffer;
    void *pixels;
    const char *app_id;
    bool configured;
    bool fullscreen_configured;
    int32_t configured_width;
    int32_t configured_height;
    bool close_requested;
};

struct test_state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct moko_window_manager_v1 *window_manager;
    struct observed_window observed[4];
    bool input_config_received;
    uint32_t input_device_count;
    uint32_t input_capabilities;
    bool desktop_config_received;
    uint32_t output_scale;
    uint32_t output_scale_capabilities;
    uint32_t keyboard_layout;
    bool shutdown_blackout_presented;
};

static int create_anonymous_file(size_t size)
{
    char path[] = "/tmp/moko-compositor-client-XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0)
        return -1;
    unlink(path);
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static void toplevel_configure(void *data,
                               struct xdg_toplevel *toplevel,
                               int32_t width,
                               int32_t height,
                               struct wl_array *states)
{
    (void)toplevel;
    struct test_window *window = data;
    window->configured_width = width;
    window->configured_height = height;
    window->fullscreen_configured = false;
    uint32_t *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN)
            window->fullscreen_configured = true;
    }
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    (void)toplevel;
    struct test_window *window = data;
    window->close_requested = true;
}

static void toplevel_configure_bounds(void *data,
                                      struct xdg_toplevel *toplevel,
                                      int32_t width,
                                      int32_t height)
{
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}

static void toplevel_wm_capabilities(void *data,
                                     struct xdg_toplevel *toplevel,
                                     struct wl_array *capabilities)
{
    (void)data;
    (void)toplevel;
    (void)capabilities;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
    .wm_capabilities = toplevel_wm_capabilities,
};

static void surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct test_window *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    if (!window->configured) {
        window->configured = true;
        wl_surface_attach(window->surface, window->buffer, 0, 0);
        wl_surface_damage_buffer(window->surface, 0, 0, TEST_WIDTH, TEST_HEIGHT);
        wl_surface_commit(window->surface);
    }
}

static const struct xdg_surface_listener surface_listener = {
    .configure = surface_configure,
};

static struct observed_window *observed_by_id(struct test_state *state, uint32_t id)
{
    for (size_t index = 0; index < sizeof(state->observed) / sizeof(state->observed[0]); ++index) {
        if (state->observed[index].id == id)
            return &state->observed[index];
    }
    return NULL;
}

static struct observed_window *observed_by_app_id(struct test_state *state, const char *app_id)
{
    for (size_t index = 0; index < sizeof(state->observed) / sizeof(state->observed[0]); ++index) {
        if (state->observed[index].present
            && strcmp(state->observed[index].app_id, app_id) == 0) {
            return &state->observed[index];
        }
    }
    return NULL;
}

static void manager_window(void *data,
                           struct moko_window_manager_v1 *manager,
                           uint32_t window_id,
                           const char *app_id,
                           const char *title,
                           uint32_t state_flags)
{
    (void)manager;
    (void)title;
    struct test_state *state = data;
    struct observed_window *window = observed_by_id(state, window_id);
    if (window == NULL) {
        for (size_t index = 0; index < sizeof(state->observed) / sizeof(state->observed[0]); ++index) {
            if (!state->observed[index].present) {
                window = &state->observed[index];
                break;
            }
        }
    }
    if (window == NULL)
        return;
    window->id = window_id;
    window->state = state_flags;
    window->present = true;
    snprintf(window->app_id, sizeof(window->app_id), "%s", app_id);
}

static void manager_window_removed(void *data,
                                   struct moko_window_manager_v1 *manager,
                                   uint32_t window_id)
{
    (void)manager;
    struct observed_window *window = observed_by_id(data, window_id);
    if (window != NULL)
        window->present = false;
}

static void manager_done(void *data, struct moko_window_manager_v1 *manager)
{
    (void)data;
    (void)manager;
}

static void manager_brightness_step(void *data,
                                    struct moko_window_manager_v1 *manager,
                                    int32_t delta)
{
    (void)data;
    (void)manager;
    (void)delta;
}

static void manager_input_config(void *data,
                                 struct moko_window_manager_v1 *manager,
                                 uint32_t device_count,
                                 uint32_t capabilities,
                                 uint32_t state_flags,
                                 int32_t pointer_acceleration)
{
    (void)manager;
    (void)state_flags;
    (void)pointer_acceleration;
    struct test_state *state = data;
    state->input_config_received = true;
    state->input_device_count = device_count;
    state->input_capabilities = capabilities;
}

static void manager_desktop_config(void *data,
                                   struct moko_window_manager_v1 *manager,
                                   uint32_t output_scale,
                                   uint32_t scale_capabilities,
                                   uint32_t keyboard_layout)
{
    (void)manager;
    struct test_state *state = data;
    state->desktop_config_received = true;
    state->output_scale = output_scale;
    state->output_scale_capabilities = scale_capabilities;
    state->keyboard_layout = keyboard_layout;
}

static void manager_global_action(void *data,
                                  struct moko_window_manager_v1 *manager,
                                  uint32_t action)
{
    (void)data;
    (void)manager;
    (void)action;
}

static void manager_shutdown_blackout_presented(void *data,
                                                struct moko_window_manager_v1 *manager)
{
    (void)manager;
    struct test_state *state = data;
    state->shutdown_blackout_presented = true;
}

static const struct moko_window_manager_v1_listener manager_listener = {
    .window = manager_window,
    .window_removed = manager_window_removed,
    .done = manager_done,
    .brightness_step = manager_brightness_step,
    .input_config = manager_input_config,
    .desktop_config = manager_desktop_config,
    .global_action = manager_global_action,
    .shutdown_blackout_presented = manager_shutdown_blackout_presented,
};

static void registry_global(void *data,
                            struct wl_registry *registry,
                            uint32_t name,
                            const char *interface,
                            uint32_t version)
{
    struct test_state *state = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                             version < 5 ? version : 5);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                          version < 6 ? version : 6);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, state);
    } else if (strcmp(interface, moko_window_manager_v1_interface.name) == 0) {
        state->window_manager = wl_registry_bind(registry, name,
                                                 &moko_window_manager_v1_interface,
                                                 version < 4 ? version : 4);
        moko_window_manager_v1_add_listener(state->window_manager, &manager_listener, state);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static bool create_test_window(struct test_state *state,
                               struct test_window *window,
                               const char *app_id,
                               uint32_t color)
{
    memset(window, 0, sizeof(*window));
    window->state = state;
    window->app_id = app_id;

    const int fd = create_anonymous_file(TEST_BUFFER_SIZE);
    if (fd < 0)
        return false;
    window->pixels = mmap(NULL, TEST_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (window->pixels == MAP_FAILED) {
        close(fd);
        window->pixels = NULL;
        return false;
    }
    uint32_t *pixels = window->pixels;
    for (size_t index = 0; index < TEST_BUFFER_SIZE / sizeof(uint32_t); ++index)
        pixels[index] = color;

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, TEST_BUFFER_SIZE);
    window->buffer = wl_shm_pool_create_buffer(pool, 0, TEST_WIDTH, TEST_HEIGHT,
                                               TEST_STRIDE, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    window->surface = wl_compositor_create_surface(state->compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(state->wm_base, window->surface);
    xdg_surface_add_listener(window->xdg_surface, &surface_listener, window);
    window->toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    xdg_toplevel_add_listener(window->toplevel, &toplevel_listener, window);
    xdg_toplevel_set_app_id(window->toplevel, app_id);
    xdg_toplevel_set_title(window->toplevel, app_id);
    wl_surface_commit(window->surface);
    return true;
}

static void destroy_test_window(struct test_window *window)
{
    if (window->toplevel != NULL)
        xdg_toplevel_destroy(window->toplevel);
    if (window->xdg_surface != NULL)
        xdg_surface_destroy(window->xdg_surface);
    if (window->surface != NULL)
        wl_surface_destroy(window->surface);
    if (window->buffer != NULL)
        wl_buffer_destroy(window->buffer);
    if (window->pixels != NULL)
        munmap(window->pixels, TEST_BUFFER_SIZE);
    memset(window, 0, sizeof(*window));
}

static bool dispatch_roundtrips(struct test_state *state, unsigned int count)
{
    for (unsigned int iteration = 0; iteration < count; ++iteration) {
        if (wl_display_roundtrip(state->display) < 0)
            return false;
    }
    return true;
}

static void hold_shutdown_frame_for_capture(void)
{
    const char *value = getenv("MOKO_SHUTDOWN_TEST_HOLD_MS");
    if (value == NULL || value[0] == '\0')
        return;

    char *end = NULL;
    const long milliseconds = strtol(value, &end, 10);
    if (end == value || *end != '\0' || milliseconds <= 0 || milliseconds > 5000)
        return;
    usleep((useconds_t)milliseconds * 1000);
}

static bool expect_state(struct test_state *state,
                         struct observed_window *window,
                         uint32_t required,
                         uint32_t forbidden,
                         const char *label)
{
    if (!dispatch_roundtrips(state, 2))
        return false;
    if (!window->present || (window->state & required) != required
        || (window->state & forbidden) != 0) {
        fprintf(stderr, "%s: unexpected state=%u required=%u forbidden=%u\n",
                label, window->state, required, forbidden);
        return false;
    }
    return true;
}

int main(void)
{
    struct test_state state = {0};
    struct test_window shell = {0};
    struct test_window first = {0};
    struct test_window second = {0};
    bool success = false;

    state.display = wl_display_connect(NULL);
    if (state.display == NULL) {
        fprintf(stderr, "Could not connect to the test compositor: %s\n", strerror(errno));
        goto cleanup;
    }
    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    if (!dispatch_roundtrips(&state, 2)
        || state.compositor == NULL || state.shm == NULL
        || state.wm_base == NULL || state.window_manager == NULL) {
        fputs("Required Wayland globals are unavailable.\n", stderr);
        goto cleanup;
    }
    if (!state.input_config_received || state.input_device_count != 0
        || state.input_capabilities != 0) {
        fputs("Headless compositor reported unexpected touchpad hardware.\n", stderr);
        goto cleanup;
    }
    if (!state.desktop_config_received || state.output_scale != 100
        || state.keyboard_layout != MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH
        || (state.output_scale_capabilities
            & MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_100) == 0
        || (state.output_scale_capabilities
            & MOKO_WINDOW_MANAGER_V1_OUTPUT_SCALE_CAPABILITY_SCALE_200) != 0) {
        fprintf(stderr,
                "Unexpected desktop config: received=%d scale=%u capabilities=%u layout=%u.\n",
                state.desktop_config_received,
                state.output_scale,
                state.output_scale_capabilities,
                state.keyboard_layout);
        goto cleanup;
    }
    moko_window_manager_v1_set_natural_scroll(state.window_manager, 0);
    moko_window_manager_v1_set_pointer_acceleration(state.window_manager, 5000);
    if (!dispatch_roundtrips(&state, 2) || !state.input_config_received) {
        fputs("Runtime input configuration requests failed.\n", stderr);
        goto cleanup;
    }
    moko_window_manager_v1_set_output_scale(state.window_manager, 150);
    moko_window_manager_v1_set_output_scale(state.window_manager, 200);
    if (!dispatch_roundtrips(&state, 2) || state.output_scale != 100) {
        fputs("Unsupported output scale changed the desktop configuration.\n", stderr);
        goto cleanup;
    }
    moko_window_manager_v1_set_keyboard_layout(
        state.window_manager, MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE);
    if (!dispatch_roundtrips(&state, 2)
        || state.keyboard_layout != MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_VIETNAMESE) {
        fputs("Vietnamese keyboard layout request was not applied.\n", stderr);
        goto cleanup;
    }
    moko_window_manager_v1_set_keyboard_layout(
        state.window_manager, MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH);
    if (!dispatch_roundtrips(&state, 2)
        || state.keyboard_layout != MOKO_WINDOW_MANAGER_V1_KEYBOARD_LAYOUT_ENGLISH) {
        fputs("English keyboard layout request was not restored.\n", stderr);
        goto cleanup;
    }

    if (!create_test_window(&state, &shell, "org.moko.Shell", 0xffe8f4ff)
        || !dispatch_roundtrips(&state, 5)
        || !shell.fullscreen_configured
        || shell.configured_width <= 0
        || shell.configured_height <= 0) {
        fprintf(stderr, "Shell did not receive an output-sized fullscreen configure (%dx%d, fullscreen=%d).\n",
                shell.configured_width,
                shell.configured_height,
                shell.fullscreen_configured);
        goto cleanup;
    }
    moko_window_manager_v1_set_shell_overlay(state.window_manager, 1);
    moko_window_manager_v1_set_shell_overlay(state.window_manager, 0);
    if (!dispatch_roundtrips(&state, 2)) {
        fputs("Shell overlay requests failed.\n", stderr);
        goto cleanup;
    }

    if (!create_test_window(&state, &first, "org.moko.TestOne", 0xff3978f6)
        || !create_test_window(&state, &second, "org.moko.TestTwo", 0xffeaf4fc)
        || !dispatch_roundtrips(&state, 5)) {
        fputs("Could not map two XDG toplevels.\n", stderr);
        goto cleanup;
    }

    struct observed_window *first_observed = observed_by_app_id(&state, first.app_id);
    struct observed_window *second_observed = observed_by_app_id(&state, second.app_id);
    if (first_observed == NULL || second_observed == NULL) {
        fputs("The MOKO protocol did not enumerate both windows.\n", stderr);
        goto cleanup;
    }

    moko_window_manager_v1_activate(state.window_manager, first_observed->id);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED,
                      MOKO_WINDOW_MANAGER_V1_STATE_MINIMIZED,
                      "activate"))
        goto cleanup;

    moko_window_manager_v1_set_minimized(state.window_manager, first_observed->id, 1);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_MINIMIZED,
                      MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED,
                      "minimize"))
        goto cleanup;

    moko_window_manager_v1_activate(state.window_manager, first_observed->id);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_ACTIVATED,
                      MOKO_WINDOW_MANAGER_V1_STATE_MINIMIZED,
                      "restore"))
        goto cleanup;

    moko_window_manager_v1_set_maximized(state.window_manager, first_observed->id, 1);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_MAXIMIZED,
                      MOKO_WINDOW_MANAGER_V1_STATE_FULLSCREEN,
                      "maximize"))
        goto cleanup;
    moko_window_manager_v1_set_maximized(state.window_manager, first_observed->id, 0);
    if (!expect_state(&state, first_observed, 0,
                      MOKO_WINDOW_MANAGER_V1_STATE_MAXIMIZED,
                      "unmaximize"))
        goto cleanup;

    moko_window_manager_v1_set_fullscreen(state.window_manager, first_observed->id, 1);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_FULLSCREEN,
                      MOKO_WINDOW_MANAGER_V1_STATE_MAXIMIZED,
                      "fullscreen"))
        goto cleanup;
    moko_window_manager_v1_set_fullscreen(state.window_manager, first_observed->id, 0);
    if (!expect_state(&state, first_observed, 0,
                      MOKO_WINDOW_MANAGER_V1_STATE_FULLSCREEN,
                      "leave fullscreen"))
        goto cleanup;

    moko_window_manager_v1_snap(state.window_manager, first_observed->id,
                                MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_LEFT);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_LEFT,
                      MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_RIGHT,
                      "snap left"))
        goto cleanup;
    moko_window_manager_v1_snap(state.window_manager, first_observed->id,
                                MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_RIGHT);
    if (!expect_state(&state, first_observed,
                      MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_RIGHT,
                      MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_LEFT,
                      "snap right"))
        goto cleanup;
    moko_window_manager_v1_snap(state.window_manager, first_observed->id,
                                MOKO_WINDOW_MANAGER_V1_SNAP_SIDE_NONE);
    if (!expect_state(&state, first_observed, 0,
                      MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_LEFT
                          | MOKO_WINDOW_MANAGER_V1_STATE_SNAPPED_RIGHT,
                      "unsnap"))
        goto cleanup;

    const uint32_t second_id = second_observed->id;
    moko_window_manager_v1_close(state.window_manager, second_id);
    if (!dispatch_roundtrips(&state, 2) || !second.close_requested) {
        fputs("Close did not reach the XDG toplevel.\n", stderr);
        goto cleanup;
    }
    destroy_test_window(&second);
    if (!dispatch_roundtrips(&state, 3)
        || (observed_by_id(&state, second_id) != NULL
            && observed_by_id(&state, second_id)->present)) {
        fputs("Closed window was not removed from the MOKO protocol.\n", stderr);
        goto cleanup;
    }

    moko_window_manager_v1_prepare_shutdown(state.window_manager);
    for (unsigned int attempt = 0;
         attempt < 100 && !state.shutdown_blackout_presented;
         ++attempt) {
        if (!dispatch_roundtrips(&state, 1))
            goto cleanup;
        usleep(10000);
    }
    if (!state.shutdown_blackout_presented) {
        fputs("Compositor did not confirm its shutdown black frame.\n", stderr);
        goto cleanup;
    }
    hold_shutdown_frame_for_capture();

    puts("MOKO compositor headless multi-window integration passed.");
    success = true;

cleanup:
    destroy_test_window(&second);
    destroy_test_window(&first);
    destroy_test_window(&shell);
    if (state.window_manager != NULL)
        moko_window_manager_v1_destroy(state.window_manager);
    if (state.wm_base != NULL)
        xdg_wm_base_destroy(state.wm_base);
    if (state.shm != NULL)
        wl_shm_destroy(state.shm);
    if (state.compositor != NULL)
        wl_compositor_destroy(state.compositor);
    if (state.registry != NULL)
        wl_registry_destroy(state.registry);
    if (state.display != NULL)
        wl_display_disconnect(state.display);
    return success ? 0 : 1;
}
