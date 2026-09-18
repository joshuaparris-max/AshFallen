#include "shell.h"
#include "gfx.h"
#include "keyboard.h"

#define MAX_LINES 18
#define MAX_LINE 80
#define MAX_INPUT 48

static char lines[MAX_LINES][MAX_LINE];
static int line_count;
static char input[MAX_INPUT];
static int input_len;
static int sx, sy, sw, sh;
static uint64_t mem_mib;

static int lower(char c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int equals(const char *a, const char *b) {
    while (*a && *b) {
        if (lower(*a++) != lower(*b++)) return 0;
    }
    return *a == 0 && *b == 0;
}

static int starts_with(const char *text, const char *prefix) {
    while (*prefix) {
        if (lower(*text++) != lower(*prefix++)) return 0;
    }
    return 1;
}

static void copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src && src[i] && i < max - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void push(const char *text) {
    if (line_count == MAX_LINES) {
        for (int i = 1; i < MAX_LINES; ++i)
            copy(lines[i - 1], lines[i], MAX_LINE);
        --line_count;
    }
    copy(lines[line_count++], text, MAX_LINE);
}

static void number(uint64_t value, char *out) {
    char temp[24];
    int n = 0;
    if (value == 0) temp[n++] = '0';
    while (value && n < (int)sizeof(temp)) {
        temp[n++] = (char)('0' + value % 10);
        value /= 10;
    }
    int i = 0;
    while (n) out[i++] = temp[--n];
    out[i] = 0;
}

static void render(void) {
    uint32_t bg = gfx_rgb(10, 17, 31);
    uint32_t fg = gfx_rgb(218, 230, 246);
    uint32_t dim = gfx_rgb(123, 146, 178);
    uint32_t accent = gfx_rgb(91, 218, 255);
    gfx_rect(sx, sy, sw, sh, bg);

    int scale = (sw >= 900 && sh >= 420) ? 2 : 1;
    int row_h = 10 * scale;
    int visible = (sh - 30) / row_h;
    if (visible > MAX_LINES) visible = MAX_LINES;
    int first = line_count > visible ? line_count - visible : 0;
    int y = sy + 10;
    for (int i = first; i < line_count; ++i) {
        gfx_text(sx + 10, y, lines[i], scale, dim);
        y += row_h;
    }

    char prompt[MAX_LINE] = "JOSH@JOSHOS:~> ";
    int p = 15;
    for (int i = 0; input[i] && p < MAX_LINE - 2; ++i) prompt[p++] = input[i];
    prompt[p++] = '_';
    prompt[p] = 0;
    if (y + row_h <= sy + sh)
        gfx_text(sx + 10, y, prompt, scale, accent);
    else
        gfx_text(sx + 10, sy + sh - row_h - 4, prompt, scale, accent);
    (void)fg;
}

static void execute(void) {
    char echo[MAX_LINE];
    copy(echo, "JOSH@JOSHOS:~> ", MAX_LINE);
    int p = 15;
    for (int i = 0; input[i] && p < MAX_LINE - 1; ++i) echo[p++] = input[i];
    echo[p] = 0;
    push(echo);

    if (input_len == 0) {
        /* just a new prompt */
    } else if (equals(input, "help")) {
        push("HELP   LIST COMMANDS");
        push("ABOUT  ABOUT JOSH OS");
        push("MEM    SHOW DETECTED MEMORY");
        push("CLEAR  CLEAR TERMINAL");
        push("ECHO   PRINT TEXT");
        push("REBOOT RESTART COMPUTER");
    } else if (equals(input, "about")) {
        push("JOSH OS 0.1 PRE-ALPHA");
        push("A TINY INDEPENDENT X86-64 OPERATING SYSTEM.");
        push("NO LINUX KERNEL UNDERNEATH.");
    } else if (equals(input, "mem")) {
        char value[24];
        char line[MAX_LINE] = "USABLE MEMORY: ";
        number(mem_mib, value);
        int i = 15, j = 0;
        while (value[j] && i < MAX_LINE - 5) line[i++] = value[j++];
        line[i++] = ' ';
        line[i++] = 'M';
        line[i++] = 'I';
        line[i++] = 'B';
        line[i] = 0;
        push(line);
    } else if (equals(input, "clear")) {
        line_count = 0;
    } else if (equals(input, "reboot")) {
        push("REBOOTING...");
        render();
        keyboard_reboot();
    } else if (starts_with(input, "echo ")) {
        push(input + 5);
    } else {
        push("COMMAND NOT FOUND. TYPE HELP.");
    }

    input_len = 0;
    input[0] = 0;
    render();
}

void shell_init(int x, int y, int w, int h, uint64_t memory_mib) {
    sx = x; sy = y; sw = w; sh = h; mem_mib = memory_mib;
    line_count = 0;
    input_len = 0;
    push("JOSH OS GRAPHICAL SHELL");
    push("KERNEL ONLINE. TYPE HELP TO EXPLORE.");
    push("");
    render();
}

void shell_handle_key(char key) {
    if (!key) return;
    if (key == '\n') {
        execute();
        return;
    }
    if (key == '\b') {
        if (input_len > 0) input[--input_len] = 0;
        render();
        return;
    }
    if (key >= 32 && key <= 126 && input_len < MAX_INPUT - 1) {
        input[input_len++] = key;
        input[input_len] = 0;
        render();
    }
}
