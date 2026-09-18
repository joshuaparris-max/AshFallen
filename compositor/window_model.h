#ifndef JOSH_WINDOW_MODEL_H
#define JOSH_WINDOW_MODEL_H

#include <stdbool.h>

enum josh_snap_zone {
    JOSH_SNAP_NONE = 0,
    JOSH_SNAP_MAXIMISE,
    JOSH_SNAP_LEFT,
    JOSH_SNAP_RIGHT,
};

struct josh_rect {
    int x;
    int y;
    int width;
    int height;
};

struct josh_window_state {
    struct josh_rect geometry;
    struct josh_rect previous_geometry;
    bool has_previous_geometry;
    bool maximised;
    bool minimised;
};

enum josh_snap_zone josh_snap_zone_for(
    const struct josh_rect *desktop,
    double pointer_x,
    double pointer_y,
    int edge_threshold);

struct josh_rect josh_snap_geometry(
    const struct josh_rect *desktop,
    enum josh_snap_zone zone);

void josh_window_toggle_maximise(
    struct josh_window_state *window,
    const struct josh_rect *desktop);

void josh_window_apply_snap(
    struct josh_window_state *window,
    const struct josh_rect *desktop,
    enum josh_snap_zone zone);

#endif
