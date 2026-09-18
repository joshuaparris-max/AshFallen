#include "desktop.h"
#include "gfx.h"

static void dot(int x, int y, uint32_t colour) {
    gfx_round_rect(x, y, 10, 10, 5, colour);
}

static void dock_icon(int x, int y, char letter, uint32_t fill, uint32_t text) {
    char label[2] = { letter, 0 };
    gfx_round_rect(x, y, 42, 42, 11, fill);
    gfx_text(x + 15, y + 13, label, 2, text);
}

desktop_layout_t desktop_draw(void) {
    int sw = (int)gfx_width();
    int sh = (int)gfx_height();
    uint32_t white = gfx_rgb(235, 243, 255);
    uint32_t muted = gfx_rgb(154, 173, 204);
    uint32_t panel = gfx_rgb(16, 25, 45);
    uint32_t panel2 = gfx_rgb(20, 31, 54);
    uint32_t border = gfx_rgb(43, 61, 91);
    uint32_t cyan = gfx_rgb(86, 211, 255);
    uint32_t violet = gfx_rgb(158, 116, 255);

    gfx_clear_gradient();

    gfx_rect(0, 0, sw, 42, gfx_rgb(8, 13, 25));
    gfx_text(22, 14, "JOSH OS", 2, white);
    gfx_text(sw - 152, 16, "PRE-ALPHA 0.1", 1, muted);

    int margin = sw / 14;
    if (margin < 34) margin = 34;
    int content_top = 76;

    gfx_text(margin, content_top, "WELCOME TO JOSH OS", sw >= 1200 ? 3 : 2, white);
    gfx_text(margin, content_top + (sw >= 1200 ? 30 : 22), "SIMPLE. OPEN. UNDERSTANDABLE.", 1, cyan);

    int side_w = sw >= 900 ? sw / 4 : 0;
    int gap = side_w ? 22 : 0;
    int win_x = margin + side_w + gap;
    int win_y = content_top + 62;
    int win_w = sw - win_x - margin;
    int win_h = sh - win_y - 108;

    if (side_w) {
        int side_h = win_h;
        gfx_panel(margin, win_y, side_w, side_h, 18, panel, border);
        gfx_text(margin + 20, win_y + 22, "SYSTEM", 2, white);
        gfx_text(margin + 20, win_y + 52, "KERNEL", 1, muted);
        gfx_text(margin + 20, win_y + 69, "JOSH KERNEL 0.1", 1, cyan);
        gfx_text(margin + 20, win_y + 99, "BOOT", 1, muted);
        gfx_text(margin + 20, win_y + 116, "LIMINE 12.9", 1, cyan);
        gfx_text(margin + 20, win_y + 146, "DISPLAY", 1, muted);
        gfx_text(margin + 20, win_y + 163, "FRAMEBUFFER", 1, cyan);

        int card_y = win_y + side_h - 126;
        gfx_round_rect(margin + 14, card_y, side_w - 28, 104, 14, panel2);
        gfx_text(margin + 28, card_y + 18, "PRINCIPLE 01", 1, violet);
        gfx_text(margin + 28, card_y + 40, "THE SYSTEM", 1, white);
        gfx_text(margin + 28, card_y + 56, "SHOULD BECOME", 1, white);
        gfx_text(margin + 28, card_y + 72, "CLEARER AS YOU", 1, white);
        gfx_text(margin + 28, card_y + 88, "UNDERSTAND IT.", 1, white);
    } else {
        win_x = margin;
        win_w = sw - margin * 2;
    }

    gfx_panel(win_x, win_y, win_w, win_h, 18, panel, border);
    gfx_rect(win_x + 1, win_y + 42, win_w - 2, 1, border);
    dot(win_x + 18, win_y + 16, gfx_rgb(255, 105, 112));
    dot(win_x + 36, win_y + 16, gfx_rgb(255, 201, 92));
    dot(win_x + 54, win_y + 16, gfx_rgb(94, 220, 146));
    gfx_text(win_x + 82, win_y + 16, "TERMINAL", 1, muted);

    int dock_w = 246;
    int dock_x = (sw - dock_w) / 2;
    int dock_y = sh - 68;
    gfx_panel(dock_x, dock_y, dock_w, 54, 18, gfx_rgb(13, 21, 38), border);
    dock_icon(dock_x + 12, dock_y + 6, 'T', gfx_rgb(37, 60, 91), white);
    dock_icon(dock_x + 60, dock_y + 6, 'F', gfx_rgb(42, 91, 117), white);
    dock_icon(dock_x + 108, dock_y + 6, 'N', gfx_rgb(84, 66, 133), white);
    dock_icon(dock_x + 156, dock_y + 6, 'S', gfx_rgb(50, 75, 103), white);
    dock_icon(dock_x + 204, dock_y + 6, 'J', gfx_rgb(41, 127, 151), white);

    desktop_layout_t layout = {
        .terminal_x = win_x + 18,
        .terminal_y = win_y + 56,
        .terminal_w = win_w - 36,
        .terminal_h = win_h - 72
    };
    return layout;
}
