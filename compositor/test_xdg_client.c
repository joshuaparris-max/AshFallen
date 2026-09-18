#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

struct client_state {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_buffer *buffer;
    int configured;
    int closed;
};

static int create_shm_file(size_t size) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime == NULL) {
        errno = ENOENT;
        return -1;
    }

    char path[512];
    int written = snprintf(path, sizeof(path), "%s/josh-xdg-test-XXXXXX", runtime);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }
    unlink(path);

    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static struct wl_buffer *create_buffer(
    struct client_state *state,
    int width,
    int height) {
    const int stride = width * 4;
    const size_t size = (size_t)stride * (size_t)height;
    int fd = create_shm_file(size);
    if (fd < 0) {
        perror("create shm file");
        return NULL;
    }

    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t shade = (uint32_t)(0x20 + ((x + y) % 0x30));
            pixels[(size_t)y * (size_t)width + (size_t)x] =
                0xff000000u | (shade << 16) | (shade << 8) | (shade + 0x20u);
        }
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, (int)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    munmap(pixels, size);
    close(fd);
    return buffer;
}

static void wm_base_ping(
    void *data,
    struct xdg_wm_base *wm_base,
    uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static void xdg_surface_configure(
    void *data,
    struct xdg_surface *xdg_surface,
    uint32_t serial) {
    struct client_state *state = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    if (state->buffer == NULL) {
        state->buffer = create_buffer(state, 320, 200);
        if (state->buffer == NULL) {
            state->closed = 1;
            return;
        }
    }

    wl_surface_attach(state->surface, state->buffer, 0, 0);
    wl_surface_damage(state->surface, 0, 0, 320, 200);
    wl_surface_commit(state->surface);
    state->configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void toplevel_configure(
    void *data,
    struct xdg_toplevel *toplevel,
    int32_t width,
    int32_t height,
    struct wl_array *states) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
    (void)states;
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)toplevel;
    struct client_state *state = data;
    state->closed = 1;
}

static void toplevel_configure_bounds(
    void *data,
    struct xdg_toplevel *toplevel,
    int32_t width,
    int32_t height) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}

static void toplevel_wm_capabilities(
    void *data,
    struct xdg_toplevel *toplevel,
    struct wl_array *capabilities) {
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

static void registry_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version) {
    struct client_state *state = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        uint32_t bind_version = version < 4 ? version : 4;
        state->compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, bind_version);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(
            registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        uint32_t bind_version = version < 6 ? version : 6;
        state->wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, bind_version);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, state);
    }
}

static void registry_global_remove(
    void *data,
    struct wl_registry *registry,
    uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(void) {
    struct client_state state = {0};

    state.display = wl_display_connect(NULL);
    if (state.display == NULL) {
        fprintf(stderr, "josh-xdg-test: cannot connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(registry, &registry_listener, &state);
    if (wl_display_roundtrip(state.display) < 0) {
        fprintf(stderr, "josh-xdg-test: registry roundtrip failed\n");
        return 1;
    }

    if (state.compositor == NULL || state.shm == NULL || state.wm_base == NULL) {
        fprintf(stderr, "josh-xdg-test: required Wayland globals unavailable\n");
        return 1;
    }

    state.surface = wl_compositor_create_surface(state.compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
        state.wm_base, state.surface);
    xdg_surface_add_listener(
        state.xdg_surface, &xdg_surface_listener, &state);

    state.toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.toplevel, &toplevel_listener, &state);
    xdg_toplevel_set_title(state.toplevel, "Josh compositor CI window");
    xdg_toplevel_set_app_id(state.toplevel, "org.joshos.CompositorTest");

    wl_surface_commit(state.surface);

    while (!state.configured && !state.closed) {
        if (wl_display_dispatch(state.display) < 0) {
            fprintf(stderr, "josh-xdg-test: dispatch failed before configure\n");
            return 1;
        }
    }

    if (!state.configured) {
        fprintf(stderr, "josh-xdg-test: window closed before mapping\n");
        return 1;
    }

    if (wl_display_roundtrip(state.display) < 0) {
        fprintf(stderr, "josh-xdg-test: final roundtrip failed\n");
        return 1;
    }

    puts("JOSH_XDG_TEST_MAPPED");
    fflush(stdout);

    if (state.buffer != NULL) wl_buffer_destroy(state.buffer);
    xdg_toplevel_destroy(state.toplevel);
    xdg_surface_destroy(state.xdg_surface);
    wl_surface_destroy(state.surface);
    xdg_wm_base_destroy(state.wm_base);
    wl_shm_destroy(state.shm);
    wl_compositor_destroy(state.compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(state.display);
    return 0;
}
