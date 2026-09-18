#ifndef JOSHOS_SHELL_H
#define JOSHOS_SHELL_H

#include <stdint.h>

void shell_init(int x, int y, int w, int h, uint64_t memory_mib);
void shell_handle_key(char key);

#endif
