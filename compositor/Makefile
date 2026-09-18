CC ?= cc
CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_POSIX_C_SOURCE=200809L

.PHONY: all clean test

all: test-window-model

test-window-model: window_model.c test_window_model.c window_model.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 -o $@ window_model.c test_window_model.c

test: test-window-model
	./test-window-model

clean:
	rm -f test-window-model josh-compositor
