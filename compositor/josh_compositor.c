#define _POSIX_C_SOURCE 200809L
#define WLR_USE_UNSTABLE

#include "window_model.h"

#include <getopt.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
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
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

enum josh_cursor_mode {
    JOSH_CURSOR_PASSTHROUGH = 0,
    JOSH_CURSOR_MOVE,
    JOSH_CURSOR_RESIZE,
};

struct josh_server;
struct josh_view;

struct josh_output {
    struct josh_server *server;
    struct wlr_output *wlr_output;
    struct wlr_scene_output *scene_output;
    struct wl_listener frame;
    struct wl_listener destroy;
    struct wl_list link;
};

struct josh_keyboard {
    struct josh_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
    struct wl_list link;
};

struct josh_view {
    struct josh_server *server;
    struct wlr_xdg_toplevel *toplevel;
    struct wlr_scene_tree *scene_tree;
    struct josh_window_state state;
    bool mapped;
    bool fullscreen;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wl_listener request_minimize;
    struct wl_list link;
};

struct josh_server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;

    struct wlr_output_layout *output_layout;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wl_list outputs;
    struct wl_listener new_output;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_toplevel;
    struct wl_list views;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_manager;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_list keyboards;

    enum josh_cursor_mode cursor_mode;
    struct josh_view *grabbed_view;
    double grab_x;
    double grab_y;
    struct josh_rect grab_geometry;
    uint32_t resize_edges;

    unsigned int next_view_offset;
};

static struct josh_rect server_desktop_bounds(struct josh_server *server) {
    struct wlr_box box = {0};
    wlr_output_layout_get_box(server->output_layout, NULL, &box);
    if (box.width < 1 || box.height < 1) {
        return (struct josh_rect){ .x = 0, .y = 0, .width = 1280, .height = 720 };
    }
    return (struct josh_rect){
        .x = box.x,
        .y = box.y,
        .width = box.width,
        .height = box.height,
    };
}

static void view_apply_geometry(struct josh_view *view) {
    struct josh_rect *g = &view->state.geometry;
    wlr_scene_node_set_position(&view->scene_tree->node, g->x, g->y);
    wlr_xdg_toplevel_set_size(view->toplevel, g->width, g->height);
}

static struct josh_view *view_from_surface(struct wlr_surface *surface) {
    struct wlr_xdg_toplevel *toplevel =
        wlr_xdg_toplevel_try_from_wlr_surface(surface);
    if (toplevel == NULL || toplevel->base == NULL) {
        return NULL;
    }
    return toplevel->base->data;
}

static struct josh_view *view_from_scene_node(struct wlr_scene_node *node) {
    while (node != NULL) {
        if (node->data != NULL) {
            return node->data;
        }
        if (node->parent == NULL) {
            return NULL;
        }
        node = &node->parent->node;
    }
    return NULL;
}

static struct wlr_surface *desktop_surface_at(
    struct josh_server *server,
    double lx,
    double ly,
    double *sx,
    double *sy,
    struct josh_view **view_out) {
    struct wlr_scene_node *node =
        wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
    if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
        *view_out = NULL;
        return NULL;
    }

    struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(buffer);
    if (scene_surface == NULL) {
        *view_out = NULL;
        return NULL;
    }

    *view_out = view_from_scene_node(node);
    return scene_surface->surface;
}

static void focus_view(struct josh_view *view, struct wlr_surface *surface) {
    if (view == NULL || !view->mapped || surface == NULL) {
        return;
    }

    struct josh_server *server = view->server;
    struct wlr_surface *previous = server->seat->keyboard_state.focused_surface;
    if (previous == surface) {
        return;
    }

    if (previous != NULL) {
        struct josh_view *previous_view = view_from_surface(previous);
        if (previous_view != NULL) {
            wlr_xdg_toplevel_set_activated(previous_view->toplevel, false);
        }
    }

    wlr_scene_node_raise_to_top(&view->scene_tree->node);
    wlr_xdg_toplevel_set_activated(view->toplevel, true);

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    if (keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(
            server->seat,
            surface,
            keyboard->keycodes,
            keyboard->num_keycodes,
            &keyboard->modifiers);
    }

    wlr_log(WLR_INFO, "JOSH_COMPOSITOR_FOCUS app=%s title=%s",
        view->toplevel->app_id != NULL ? view->toplevel->app_id : "(unknown)",
        view->toplevel->title != NULL ? view->toplevel->title : "(untitled)");
}

static void process_cursor_motion(struct josh_server *server, uint32_t time_msec) {
    if (server->cursor_mode == JOSH_CURSOR_MOVE &&
        server->grabbed_view != NULL) {
        struct josh_view *view = server->grabbed_view;
        view->state.geometry.x = (int)(server->cursor->x - server->grab_x);
        view->state.geometry.y = (int)(server->cursor->y - server->grab_y);
        wlr_scene_node_set_position(
            &view->scene_tree->node,
            view->state.geometry.x,
            view->state.geometry.y);
        return;
    }

    if (server->cursor_mode == JOSH_CURSOR_RESIZE &&
        server->grabbed_view != NULL) {
        struct josh_view *view = server->grabbed_view;
        struct josh_rect next = server->grab_geometry;
        int dx = (int)(server->cursor->x - server->grab_x);
        int dy = (int)(server->cursor->y - server->grab_y);

        if ((server->resize_edges & WLR_EDGE_LEFT) != 0) {
            next.x += dx;
            next.width -= dx;
        } else if ((server->resize_edges & WLR_EDGE_RIGHT) != 0) {
            next.width += dx;
        }

        if ((server->resize_edges & WLR_EDGE_TOP) != 0) {
            next.y += dy;
            next.height -= dy;
        } else if ((server->resize_edges & WLR_EDGE_BOTTOM) != 0) {
            next.height += dy;
        }

        if (next.width < 100) next.width = 100;
        if (next.height < 80) next.height = 80;

        view->state.geometry = next;
        view_apply_geometry(view);
        return;
    }

    double sx = 0.0;
    double sy = 0.0;
    struct josh_view *view = NULL;
    struct wlr_surface *surface = desktop_surface_at(
        server, server->cursor->x, server->cursor->y, &sx, &sy, &view);

    if (surface == NULL) {
        wlr_cursor_set_xcursor(server->cursor, server->cursor_manager, "default");
        wlr_seat_pointer_notify_clear_focus(server->seat);
        return;
    }

    wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);
}

static void end_interactive(struct josh_server *server) {
    if (server->cursor_mode == JOSH_CURSOR_MOVE &&
        server->grabbed_view != NULL) {
        struct josh_view *view = server->grabbed_view;
        struct josh_rect desktop = server_desktop_bounds(server);
        enum josh_snap_zone zone = josh_snap_zone_for(
            &desktop, server->cursor->x, server->cursor->y, 24);
        if (zone != JOSH_SNAP_NONE) {
            josh_window_apply_snap(&view->state, &desktop, zone);
            view_apply_geometry(view);

            if (zone == JOSH_SNAP_MAXIMISE) {
                wlr_xdg_toplevel_set_maximized(view->toplevel, true);
                wlr_xdg_toplevel_set_tiled(view->toplevel, 0);
            } else {
                wlr_xdg_toplevel_set_maximized(view->toplevel, false);
                uint32_t tiled = WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
                tiled |= zone == JOSH_SNAP_LEFT ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
                wlr_xdg_toplevel_set_tiled(view->toplevel, tiled);
            }
            wlr_log(WLR_INFO, "JOSH_COMPOSITOR_SNAP zone=%d", zone);
        }
    }

    if (server->cursor_mode == JOSH_CURSOR_RESIZE &&
        server->grabbed_view != NULL) {
        wlr_xdg_toplevel_set_resizing(server->grabbed_view->toplevel, false);
    }

    server->cursor_mode = JOSH_CURSOR_PASSTHROUGH;
    server->grabbed_view = NULL;
    server->resize_edges = 0;
}

static bool begin_interactive(
    struct josh_view *view,
    enum josh_cursor_mode mode,
    uint32_t edges,
    uint32_t serial) {
    struct josh_server *server = view->server;
    if (!wlr_seat_validate_pointer_grab_serial(
            server->seat, view->toplevel->base->surface, serial)) {
        return false;
    }

    if (view->state.maximised && mode == JOSH_CURSOR_MOVE) {
        josh_window_toggle_maximise(
            &view->state, &server_desktop_bounds(server));
        wlr_xdg_toplevel_set_maximized(view->toplevel, false);
        view_apply_geometry(view);
    }

    server->grabbed_view = view;
    server->cursor_mode = mode;
    server->resize_edges = edges;
    server->grab_geometry = view->state.geometry;

    if (mode == JOSH_CURSOR_MOVE) {
        server->grab_x = server->cursor->x - view->state.geometry.x;
        server->grab_y = server->cursor->y - view->state.geometry.y;
    } else {
        server->grab_x = server->cursor->x;
        server->grab_y = server->cursor->y;
        wlr_xdg_toplevel_set_resizing(view->toplevel, true);
    }

    return true;
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(
        server->cursor,
        &event->pointer->base,
        event->delta_x,
        event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_motion_absolute(
    struct wl_listener *listener,
    void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(
        server->cursor, &event->pointer->base, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double sx = 0.0;
        double sy = 0.0;
        struct josh_view *view = NULL;
        struct wlr_surface *surface = desktop_surface_at(
            server, server->cursor->x, server->cursor->y, &sx, &sy, &view);
        if (view != NULL && surface != NULL) {
            focus_view(view, surface);
        }
    }

    wlr_seat_pointer_notify_button(
        server->seat, event->time_msec, event->button, event->state);
    wlr_seat_pointer_notify_frame(server->seat);

    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        end_interactive(server);
    }
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(
        server->seat,
        event->time_msec,
        event->orientation,
        event->delta,
        event->delta_discrete,
        event->source,
        event->relative_direction);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_server *server =
        wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

static bool handle_keybinding(
    struct josh_server *server,
    struct wlr_keyboard *keyboard,
    xkb_keysym_t sym) {
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
    struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
    struct josh_view *view = focused != NULL ? view_from_surface(focused) : NULL;

    if ((modifiers & WLR_MODIFIER_ALT) != 0 && sym == XKB_KEY_F4) {
        if (view != NULL) {
            wlr_xdg_toplevel_send_close(view->toplevel);
        }
        return true;
    }

    if ((modifiers & WLR_MODIFIER_LOGO) == 0 || view == NULL) {
        return false;
    }

    struct josh_rect desktop = server_desktop_bounds(server);
    switch (sym) {
    case XKB_KEY_Up:
        if (!view->state.maximised) {
            josh_window_toggle_maximise(&view->state, &desktop);
        }
        wlr_xdg_toplevel_set_maximized(view->toplevel, true);
        wlr_xdg_toplevel_set_tiled(view->toplevel, 0);
        view_apply_geometry(view);
        return true;
    case XKB_KEY_Down:
        if (view->state.maximised) {
            josh_window_toggle_maximise(&view->state, &desktop);
            wlr_xdg_toplevel_set_maximized(view->toplevel, false);
            view_apply_geometry(view);
            return true;
        }
        return false;
    case XKB_KEY_Left:
        josh_window_apply_snap(&view->state, &desktop, JOSH_SNAP_LEFT);
        wlr_xdg_toplevel_set_maximized(view->toplevel, false);
        wlr_xdg_toplevel_set_tiled(
            view->toplevel, WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM);
        view_apply_geometry(view);
        return true;
    case XKB_KEY_Right:
        josh_window_apply_snap(&view->state, &desktop, JOSH_SNAP_RIGHT);
        wlr_xdg_toplevel_set_maximized(view->toplevel, false);
        wlr_xdg_toplevel_set_tiled(
            view->toplevel, WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM);
        view_apply_geometry(view);
        return true;
    default:
        return false;
    }
}

static void handle_keyboard_modifiers(
    struct wl_listener *listener,
    void *data) {
    (void)data;
    struct josh_keyboard *keyboard =
        wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(
        keyboard->server->seat, &keyboard->wlr_keyboard->modifiers);
}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
    struct josh_keyboard *keyboard =
        wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;

    bool handled = false;
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        const xkb_keysym_t *syms = NULL;
        int count = xkb_state_key_get_syms(
            keyboard->wlr_keyboard->xkb_state,
            event->keycode + 8,
            &syms);
        for (int i = 0; i < count; ++i) {
            handled |= handle_keybinding(
                keyboard->server, keyboard->wlr_keyboard, syms[i]);
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(
            keyboard->server->seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(
            keyboard->server->seat,
            event->time_msec,
            event->keycode,
            event->state);
    }
}

static void handle_keyboard_destroy(
    struct wl_listener *listener,
    void *data) {
    (void)data;
    struct josh_keyboard *keyboard =
        wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void new_keyboard(
    struct josh_server *server,
    struct wlr_input_device *device) {
    struct josh_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    if (keyboard == NULL) {
        return;
    }

    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard_from_input_device(device);

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = context != NULL
        ? xkb_keymap_new_from_names(
            context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS)
        : NULL;
    if (keymap != NULL) {
        wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
        xkb_keymap_unref(keymap);
    }
    if (context != NULL) {
        xkb_context_unref(context);
    }

    wlr_keyboard_set_repeat_info(keyboard->wlr_keyboard, 25, 600);

    keyboard->modifiers.notify = handle_keyboard_modifiers;
    wl_signal_add(
        &keyboard->wlr_keyboard->events.modifiers,
        &keyboard->modifiers);
    keyboard->key.notify = handle_keyboard_key;
    wl_signal_add(&keyboard->wlr_keyboard->events.key, &keyboard->key);
    keyboard->destroy.notify = handle_keyboard_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

    wl_list_insert(&server->keyboards, &keyboard->link);
    wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        wlr_cursor_attach_input_device(server->cursor, device);
        break;
    default:
        break;
    }

    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, capabilities);
}

static void handle_request_cursor(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;

    if (server->seat->pointer_state.focused_client == event->seat_client) {
        wlr_cursor_set_surface(
            server->cursor,
            event->surface,
            event->hotspot_x,
            event->hotspot_y);
    }
}

static void handle_request_set_selection(
    struct wl_listener *listener,
    void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void handle_view_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_view *view = wl_container_of(listener, view, map);
    view->mapped = true;

    struct wlr_box geometry = view->toplevel->base->geometry;
    if (geometry.width > 0 && geometry.height > 0) {
        view->state.geometry.width = geometry.width;
        view->state.geometry.height = geometry.height;
    }

    wlr_scene_node_set_enabled(&view->scene_tree->node, true);
    focus_view(view, view->toplevel->base->surface);
    wlr_log(WLR_INFO, "JOSH_COMPOSITOR_VIEW_MAPPED app=%s title=%s",
        view->toplevel->app_id != NULL ? view->toplevel->app_id : "(unknown)",
        view->toplevel->title != NULL ? view->toplevel->title : "(untitled)");
}

static void handle_view_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_view *view = wl_container_of(listener, view, unmap);
    view->mapped = false;
    wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    if (view->server->grabbed_view == view) {
        end_interactive(view->server);
    }
}

static void handle_view_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_view *view = wl_container_of(listener, view, destroy);

    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_move.link);
    wl_list_remove(&view->request_resize.link);
    wl_list_remove(&view->request_maximize.link);
    wl_list_remove(&view->request_fullscreen.link);
    wl_list_remove(&view->request_minimize.link);
    wl_list_remove(&view->link);

    view->toplevel->base->data = NULL;
    wlr_scene_node_destroy(&view->scene_tree->node);
    free(view);
}

static void handle_request_move(struct wl_listener *listener, void *data) {
    struct josh_view *view =
        wl_container_of(listener, view, request_move);
    struct wlr_xdg_toplevel_move_event *event = data;
    begin_interactive(view, JOSH_CURSOR_MOVE, 0, event->serial);
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
    struct josh_view *view =
        wl_container_of(listener, view, request_resize);
    struct wlr_xdg_toplevel_resize_event *event = data;
    begin_interactive(view, JOSH_CURSOR_RESIZE, event->edges, event->serial);
}

static void handle_request_maximize(
    struct wl_listener *listener,
    void *data) {
    (void)data;
    struct josh_view *view =
        wl_container_of(listener, view, request_maximize);
    bool requested = view->toplevel->requested.maximized;

    if (requested != view->state.maximised) {
        struct josh_rect desktop = server_desktop_bounds(view->server);
        josh_window_toggle_maximise(&view->state, &desktop);
        view_apply_geometry(view);
    }
    wlr_xdg_toplevel_set_maximized(view->toplevel, requested);
}

static void handle_request_fullscreen(
    struct wl_listener *listener,
    void *data) {
    (void)data;
    struct josh_view *view =
        wl_container_of(listener, view, request_fullscreen);
    bool requested = view->toplevel->requested.fullscreen;

    if (requested && !view->fullscreen) {
        view->state.previous_geometry = view->state.geometry;
        view->state.has_previous_geometry = true;
        view->state.geometry = server_desktop_bounds(view->server);
        view->fullscreen = true;
        view_apply_geometry(view);
    } else if (!requested && view->fullscreen) {
        if (view->state.has_previous_geometry) {
            view->state.geometry = view->state.previous_geometry;
        }
        view->fullscreen = false;
        view_apply_geometry(view);
    }

    wlr_xdg_toplevel_set_fullscreen(view->toplevel, requested);
}

static void handle_request_minimize(
    struct wl_listener *listener,
    void *data) {
    (void)data;
    struct josh_view *view =
        wl_container_of(listener, view, request_minimize);

    /*
     * xdg-shell has no compositor-to-client "minimized" state. Until the
     * Josh shell owns a task switcher which can restore hidden views, keep
     * the surface visible and send an unchanged configure rather than
     * creating an unrecoverable hidden window.
     */
    wlr_xdg_toplevel_set_size(
        view->toplevel,
        view->state.geometry.width,
        view->state.geometry.height);
}

static void handle_new_toplevel(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, new_toplevel);
    struct wlr_xdg_toplevel *toplevel = data;

    struct josh_view *view = calloc(1, sizeof(*view));
    if (view == NULL) {
        return;
    }

    view->server = server;
    view->toplevel = toplevel;
    view->scene_tree = wlr_scene_xdg_surface_create(
        &server->scene->tree, toplevel->base);
    if (view->scene_tree == NULL) {
        free(view);
        return;
    }

    int offset = 48 + (int)((server->next_view_offset++ % 8) * 28);
    view->state.geometry = (struct josh_rect){
        .x = offset,
        .y = offset,
        .width = 900,
        .height = 650,
    };
    view->scene_tree->node.data = view;
    toplevel->base->data = view;
    wlr_scene_node_set_position(
        &view->scene_tree->node,
        view->state.geometry.x,
        view->state.geometry.y);
    wlr_scene_node_set_enabled(&view->scene_tree->node, false);

    view->map.notify = handle_view_map;
    wl_signal_add(&toplevel->base->surface->events.map, &view->map);
    view->unmap.notify = handle_view_unmap;
    wl_signal_add(&toplevel->base->surface->events.unmap, &view->unmap);
    view->destroy.notify = handle_view_destroy;
    wl_signal_add(&toplevel->events.destroy, &view->destroy);

    view->request_move.notify = handle_request_move;
    wl_signal_add(&toplevel->events.request_move, &view->request_move);
    view->request_resize.notify = handle_request_resize;
    wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
    view->request_maximize.notify = handle_request_maximize;
    wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
    view->request_fullscreen.notify = handle_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
    view->request_minimize.notify = handle_request_minimize;
    wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);

    wl_list_insert(&server->views, &view->link);

    wlr_xdg_toplevel_set_wm_capabilities(
        toplevel,
        WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
        WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
        WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
    wlr_xdg_toplevel_set_size(
        toplevel, view->state.geometry.width, view->state.geometry.height);
}

static void handle_output_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_output *output =
        wl_container_of(listener, output, frame);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (wlr_scene_output_commit(output->scene_output, NULL)) {
        wlr_scene_output_send_frame_done(output->scene_output, &now);
    }
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct josh_output *output =
        wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    wlr_scene_output_destroy(output->scene_output);
    free(output);
}

static void handle_new_output(struct wl_listener *listener, void *data) {
    struct josh_server *server =
        wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    if (!wlr_output_init_render(
            wlr_output, server->allocator, server->renderer)) {
        wlr_log(WLR_ERROR, "failed to initialize renderer for output");
        return;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    if (!wlr_output_commit_state(wlr_output, &state)) {
        wlr_output_state_finish(&state);
        wlr_log(WLR_ERROR, "failed to enable output");
        return;
    }
    wlr_output_state_finish(&state);

    struct josh_output *output = calloc(1, sizeof(*output));
    if (output == NULL) {
        return;
    }
    output->server = server;
    output->wlr_output = wlr_output;
    output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
    if (output->scene_output == NULL) {
        free(output);
        return;
    }

    struct wlr_output_layout_output *layout_output =
        wlr_output_layout_add_auto(server->output_layout, wlr_output);
    if (layout_output == NULL) {
        wlr_scene_output_destroy(output->scene_output);
        free(output);
        return;
    }
    wlr_scene_output_layout_add_output(
        server->scene_layout, layout_output, output->scene_output);

    output->frame.notify = handle_output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->destroy.notify = handle_output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    wl_list_insert(&server->outputs, &output->link);

    wlr_xcursor_manager_load(server->cursor_manager, wlr_output->scale);
    wlr_log(WLR_INFO, "JOSH_COMPOSITOR_OUTPUT name=%s",
        wlr_output->name != NULL ? wlr_output->name : "(unnamed)");
}

static int run_server(const char *startup_command) {
    struct josh_server server = {0};
    wl_list_init(&server.outputs);
    wl_list_init(&server.views);
    wl_list_init(&server.keyboards);

    server.display = wl_display_create();
    if (server.display == NULL) {
        fprintf(stderr, "josh-compositor: failed to create Wayland display\n");
        return 1;
    }

    server.backend = wlr_backend_autocreate(
        wl_display_get_event_loop(server.display), NULL);
    if (server.backend == NULL) {
        fprintf(stderr, "josh-compositor: failed to create backend\n");
        wl_display_destroy(server.display);
        return 1;
    }

    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL ||
        !wlr_renderer_init_wl_display(server.renderer, server.display)) {
        fprintf(stderr, "josh-compositor: failed to initialize renderer\n");
        wl_display_destroy(server.display);
        return 1;
    }

    server.allocator = wlr_allocator_autocreate(
        server.backend, server.renderer);
    if (server.allocator == NULL) {
        fprintf(stderr, "josh-compositor: failed to create allocator\n");
        wl_display_destroy(server.display);
        return 1;
    }

    if (wlr_compositor_create(server.display, 6, server.renderer) == NULL ||
        wlr_data_device_manager_create(server.display) == NULL) {
        fprintf(stderr, "josh-compositor: failed to create core globals\n");
        wl_display_destroy(server.display);
        return 1;
    }

    server.output_layout = wlr_output_layout_create(server.display);
    server.scene = wlr_scene_create();
    if (server.output_layout == NULL || server.scene == NULL) {
        fprintf(stderr, "josh-compositor: failed to create scene\n");
        wl_display_destroy(server.display);
        return 1;
    }
    server.scene_layout =
        wlr_scene_attach_output_layout(server.scene, server.output_layout);

    server.new_output.notify = handle_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    server.xdg_shell = wlr_xdg_shell_create(server.display, 6);
    if (server.xdg_shell == NULL) {
        fprintf(stderr, "josh-compositor: failed to create xdg shell\n");
        wl_display_destroy(server.display);
        return 1;
    }
    server.new_toplevel.notify = handle_new_toplevel;
    wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_toplevel);

    server.cursor = wlr_cursor_create();
    server.cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    if (server.cursor == NULL || server.cursor_manager == NULL) {
        fprintf(stderr, "josh-compositor: failed to create cursor\n");
        wl_display_destroy(server.display);
        return 1;
    }
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    wlr_xcursor_manager_load(server.cursor_manager, 1.0f);
    wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");

    server.cursor_motion.notify = handle_cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = handle_cursor_motion_absolute;
    wl_signal_add(
        &server.cursor->events.motion_absolute,
        &server.cursor_motion_absolute);
    server.cursor_button.notify = handle_cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);
    server.cursor_axis.notify = handle_cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
    server.cursor_frame.notify = handle_cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

    server.seat = wlr_seat_create(server.display, "seat0");
    if (server.seat == NULL) {
        fprintf(stderr, "josh-compositor: failed to create seat\n");
        wl_display_destroy(server.display);
        return 1;
    }
    wlr_seat_set_capabilities(server.seat, WL_SEAT_CAPABILITY_POINTER);

    server.new_input.notify = handle_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.request_cursor.notify = handle_request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
    server.request_set_selection.notify = handle_request_set_selection;
    wl_signal_add(
        &server.seat->events.request_set_selection,
        &server.request_set_selection);

    const char *socket = wl_display_add_socket_auto(server.display);
    if (socket == NULL) {
        fprintf(stderr, "josh-compositor: failed to create Wayland socket\n");
        wl_display_destroy(server.display);
        return 1;
    }

    if (!wlr_backend_start(server.backend)) {
        fprintf(stderr, "josh-compositor: failed to start backend\n");
        wl_display_destroy(server.display);
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, 1);
    if (startup_command != NULL) {
        pid_t child = fork();
        if (child == 0) {
            execl("/bin/sh", "/bin/sh", "-c", startup_command, (char *)NULL);
            _exit(127);
        }
    }

    printf("JOSH_COMPOSITOR_READY WAYLAND_DISPLAY=%s\n", socket);
    fflush(stdout);
    wlr_log(WLR_INFO, "Josh compositor running on %s", socket);

    wl_display_run(server.display);

    wl_display_destroy_clients(server.display);
    wl_display_destroy(server.display);
    return 0;
}

int main(int argc, char **argv) {
    const char *startup_command = NULL;
    int option;

    while ((option = getopt(argc, argv, "s:h")) != -1) {
        switch (option) {
        case 's':
            startup_command = optarg;
            break;
        case 'h':
        default:
            fprintf(option == 'h' ? stdout : stderr,
                "Usage: %s [-s startup-command]\n", argv[0]);
            return option == 'h' ? 0 : 2;
        }
    }

    wlr_log_init(WLR_INFO, NULL);
    return run_server(startup_command);
}
