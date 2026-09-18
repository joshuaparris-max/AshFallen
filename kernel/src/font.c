#include "font.h"

static const uint8_t *glyph(char c) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t unknown[7] = {14,17,1,2,4,0,4};
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {31,4,4,4,4,4,31},
        {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}
    };
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t dot[7] = {0,0,0,0,0,12,12};
    static const uint8_t colon[7] = {0,12,12,0,12,12,0};
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    static const uint8_t gt[7] = {16,8,4,2,4,8,16};
    static const uint8_t lt[7] = {1,2,4,8,4,2,1};
    static const uint8_t bang[7] = {4,4,4,4,4,0,4};
    static const uint8_t underscore[7] = {0,0,0,0,0,0,31};
    static const uint8_t plus[7] = {0,4,4,31,4,4,0};
    static const uint8_t equals[7] = {0,0,31,0,31,0,0};
    static const uint8_t at[7] = {14,17,23,21,23,16,14};

    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= '0' && c <= '9') return digits[c - '0'];
    switch (c) {
        case ' ': return blank;
        case '-': return dash;
        case '.': return dot;
        case ':': return colon;
        case '/': return slash;
        case '>': return gt;
        case '<': return lt;
        case '!': return bang;
        case '_': return underscore;
        case '+': return plus;
        case '=': return equals;
        case '@': return at;
        default: return unknown;
    }
}

void font_draw_char(font_pixel_fn pixel, int x, int y, char c, int scale, uint32_t colour) {
    const uint8_t *rows = glyph(c);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1u << (4 - col))) == 0) continue;
            for (int sy = 0; sy < scale; ++sy)
                for (int sx = 0; sx < scale; ++sx)
                    pixel(x + col * scale + sx, y + row * scale + sy, colour);
        }
    }
}
