#ifndef JOSHOS_DESKTOP_H
#define JOSHOS_DESKTOP_H

typedef struct {
    int terminal_x;
    int terminal_y;
    int terminal_w;
    int terminal_h;
} desktop_layout_t;

desktop_layout_t desktop_draw(void);

#endif
