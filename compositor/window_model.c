#include "window_model.h"

static struct josh_rect normalised_desktop(const struct josh_rect *desktop) {
    struct josh_rect out = *desktop;
    if (out.width < 1) out.width = 1;
    if (out.height < 1) out.height = 1;
    return out;
}

enum josh_snap_zone josh_snap_zone_for(
    const struct josh_rect *desktop,
    double pointer_x,
    double pointer_y,
    int edge_threshold) {
    struct josh_rect b = normalised_desktop(desktop);
    if (edge_threshold < 0) edge_threshold = 0;

    if (pointer_y <= (double)(b.y + edge_threshold)) {
        return JOSH_SNAP_MAXIMISE;
    }
    if (pointer_x <= (double)(b.x + edge_threshold)) {
        return JOSH_SNAP_LEFT;
    }
    if (pointer_x >= (double)(b.x + b.width - edge_threshold)) {
        return JOSH_SNAP_RIGHT;
    }
    return JOSH_SNAP_NONE;
}

struct josh_rect josh_snap_geometry(
    const struct josh_rect *desktop,
    enum josh_snap_zone zone) {
    struct josh_rect b = normalised_desktop(desktop);
    struct josh_rect out = b;

    switch (zone) {
    case JOSH_SNAP_LEFT:
        out.width = b.width / 2;
        if (out.width < 1) out.width = 1;
        break;
    case JOSH_SNAP_RIGHT: {
        int left_width = b.width / 2;
        out.x = b.x + left_width;
        out.width = b.width - left_width;
        if (out.width < 1) out.width = 1;
        break;
    }
    case JOSH_SNAP_MAXIMISE:
        break;
    case JOSH_SNAP_NONE:
    default:
        out.width = 0;
        out.height = 0;
        break;
    }

    return out;
}

void josh_window_toggle_maximise(
    struct josh_window_state *window,
    const struct josh_rect *desktop) {
    if (window->maximised) {
        if (window->has_previous_geometry) {
            window->geometry = window->previous_geometry;
        }
        window->maximised = false;
        window->minimised = false;
        return;
    }

    window->previous_geometry = window->geometry;
    window->has_previous_geometry = true;
    window->geometry = josh_snap_geometry(desktop, JOSH_SNAP_MAXIMISE);
    window->maximised = true;
    window->minimised = false;
}

void josh_window_apply_snap(
    struct josh_window_state *window,
    const struct josh_rect *desktop,
    enum josh_snap_zone zone) {
    if (zone == JOSH_SNAP_NONE) return;

    if (!window->has_previous_geometry || window->maximised) {
        window->previous_geometry = window->geometry;
        window->has_previous_geometry = true;
    }

    window->geometry = josh_snap_geometry(desktop, zone);
    window->maximised = zone == JOSH_SNAP_MAXIMISE;
    window->minimised = false;
}
