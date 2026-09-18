#ifndef JOSHOS_GFX_H
#define JOSHOS_GFX_H

#include <stdint.h>
#include <stddef.h>
#include "boot.h"

typedef struct {
    boot_framebuffer_t fb;
    uint64_t width;
    uint64_t height;
} gfx_context_t;

void gfx_init(const boot_framebuffer_t *fb);
uint64_t gfx_width(void);
uint64_t gfx_height(void);
uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b);
void gfx_clear_gradient(void);
void gfx_rect(int x, int y, int w, int h, uint32_t colour);
void gfx_round_rect(int x, int y, int w, int h, int radius, uint32_t colour);
void gfx_panel(int x, int y, int w, int h, int radius, uint32_t fill, uint32_t border);
void gfx_text(int x, int y, const char *text, int scale, uint32_t colour);
int gfx_text_width(const char *text, int scale);

#endif
