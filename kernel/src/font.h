#ifndef JOSHOS_FONT_H
#define JOSHOS_FONT_H

#include <stdint.h>

typedef void (*font_pixel_fn)(int x, int y, uint32_t colour);
void font_draw_char(font_pixel_fn pixel, int x, int y, char c, int scale, uint32_t colour);

#endif
