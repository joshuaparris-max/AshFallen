#include "window_model.h"

#include <assert.h>
#include <stdio.h>

static void assert_rect(
    struct josh_rect actual,
    int x, int y, int width, int height) {
    assert(actual.x == x);
    assert(actual.y == y);
    assert(actual.width == width);
    assert(actual.height == height);
}

int main(void) {
    struct josh_rect desktop = { .x = 0, .y = 32, .width = 1920, .height = 1048 };

    assert(josh_snap_zone_for(&desktop, 900, 40, 24) == JOSH_SNAP_MAXIMISE);
    assert(josh_snap_zone_for(&desktop, 12, 500, 24) == JOSH_SNAP_LEFT);
    assert(josh_snap_zone_for(&desktop, 1910, 500, 24) == JOSH_SNAP_RIGHT);
    assert(josh_snap_zone_for(&desktop, 900, 500, 24) == JOSH_SNAP_NONE);

    assert_rect(josh_snap_geometry(&desktop, JOSH_SNAP_MAXIMISE),
        0, 32, 1920, 1048);
    assert_rect(josh_snap_geometry(&desktop, JOSH_SNAP_LEFT),
        0, 32, 960, 1048);
    assert_rect(josh_snap_geometry(&desktop, JOSH_SNAP_RIGHT),
        960, 32, 960, 1048);

    struct josh_window_state window = {
        .geometry = { .x = 240, .y = 180, .width = 900, .height = 650 },
    };

    josh_window_toggle_maximise(&window, &desktop);
    assert(window.maximised);
    assert(window.has_previous_geometry);
    assert_rect(window.geometry, 0, 32, 1920, 1048);

    josh_window_toggle_maximise(&window, &desktop);
    assert(!window.maximised);
    assert_rect(window.geometry, 240, 180, 900, 650);

    josh_window_apply_snap(&window, &desktop, JOSH_SNAP_LEFT);
    assert(!window.maximised);
    assert_rect(window.geometry, 0, 32, 960, 1048);

    struct josh_rect odd = { .x = 10, .y = 20, .width = 1001, .height = 700 };
    assert_rect(josh_snap_geometry(&odd, JOSH_SNAP_LEFT), 10, 20, 500, 700);
    assert_rect(josh_snap_geometry(&odd, JOSH_SNAP_RIGHT), 510, 20, 501, 700);

    puts("Josh compositor window model tests passed.");
    return 0;
}
