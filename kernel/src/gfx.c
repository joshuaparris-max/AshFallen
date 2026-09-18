#include "gfx.h"
#include "font.h"

static gfx_context_t g;

static uint32_t channel(uint8_t value, uint8_t size, uint8_t shift) {
    if (size == 0) return 0;
    uint64_t max = ((uint64_t)1 << size) - 1;
    return (uint32_t)(((uint64_t)value * max / 255) << shift);
}

uint32_t gfx_rgb(uint8_t r, uint8_t gch, uint8_t b) {
    const boot_framebuffer_t *fb = &g.fb;
    return channel(r, fb->red_mask_size, fb->red_mask_shift)
         | channel(gch, fb->green_mask_size, fb->green_mask_shift)
         | channel(b, fb->blue_mask_size, fb->blue_mask_shift);
}

void gfx_init(const boot_framebuffer_t *fb) {
    if (!fb) return;
    g.fb = *fb;
    g.width = fb->width;
    g.height = fb->height;
}

uint64_t gfx_width(void) { return g.width; }
uint64_t gfx_height(void) { return g.height; }

static void put_pixel(int x, int y, uint32_t colour) {
    if (!g.fb.address || x < 0 || y < 0 ||
        (uint64_t)x >= g.width || (uint64_t)y >= g.height) return;
    volatile uint32_t *pixels = (volatile uint32_t *)g.fb.address;
    pixels[(uint64_t)y * (g.fb.pitch / 4) + (uint64_t)x] = colour;
}

void gfx_clear_gradient(void) {
    if (!g.fb.address) return;
    for (uint64_t y = 0; y < g.height; ++y) {
        for (uint64_t x = 0; x < g.width; ++x) {
            uint32_t vx = (uint32_t)(x * 255 / (g.width ? g.width : 1));
            uint32_t vy = (uint32_t)(y * 255 / (g.height ? g.height : 1));
            uint8_t r = (uint8_t)(8 + (vy * 14) / 255 + (vx * 7) / 255);
            uint8_t gr = (uint8_t)(14 + (vy * 13) / 255 + (vx * 10) / 255);
            uint8_t b = (uint8_t)(28 + (vy * 24) / 255 + (vx * 24) / 255);

            int64_t dx = (int64_t)x - (int64_t)(g.width * 3 / 4);
            int64_t dy = (int64_t)y - (int64_t)(g.height / 4);
            uint64_t d2 = (uint64_t)(dx * dx + dy * dy);
            uint64_t reach = (g.width * g.width + g.height * g.height) / 9;
            if (d2 < reach) {
                uint32_t glow = (uint32_t)((reach - d2) * 22 / (reach ? reach : 1));
                b = (uint8_t)((b + glow > 255) ? 255 : b + glow);
                gr = (uint8_t)((gr + glow / 3 > 255) ? 255 : gr + glow / 3);
            }
            put_pixel((int)x, (int)y, gfx_rgb(r, gr, b));
        }
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t colour) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            put_pixel(x + xx, y + yy, colour);
}

static int in_rounded(int px, int py, int w, int h, int radius) {
    if (radius <= 0) return 1;
    int cx = px < radius ? radius : (px >= w - radius ? w - radius - 1 : px);
    int cy = py < radius ? radius : (py >= h - radius ? h - radius - 1 : py);
    int dx = px - cx;
    int dy = py - cy;
    return dx * dx + dy * dy <= radius * radius;
}

void gfx_round_rect(int x, int y, int w, int h, int radius, uint32_t colour) {
    if (w <= 0 || h <= 0) return;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            if (in_rounded(xx, yy, w, h, radius))
                put_pixel(x + xx, y + yy, colour);
}

void gfx_panel(int x, int y, int w, int h, int radius, uint32_t fill, uint32_t border) {
    gfx_round_rect(x + 5, y + 7, w, h, radius, gfx_rgb(4, 7, 15));
    gfx_round_rect(x, y, w, h, radius, border);
    gfx_round_rect(x + 1, y + 1, w - 2, h - 2, radius > 1 ? radius - 1 : 0, fill);
}

void gfx_text(int x, int y, const char *text, int scale, uint32_t colour) {
    if (!text || scale < 1) return;
    int pen = x;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            y += 8 * scale;
            pen = x;
            continue;
        }
        font_draw_char(put_pixel, pen, y, *p, scale, colour);
        pen += 6 * scale;
    }
}

int gfx_text_width(const char *text, int scale) {
    int n = 0;
    if (!text || scale < 1) return 0;
    while (*text++) ++n;
    return n ? n * 6 * scale - scale : 0;
}
