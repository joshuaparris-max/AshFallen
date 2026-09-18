CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
WLROOTS_PKG ?= wlroots-0.20
COMPOSITOR_PKGS := $(WLROOTS_PKG) wayland-server xkbcommon
COMPOSITOR_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(COMPOSITOR_PKGS) 2>/dev/null)
COMPOSITOR_LIBS := $(shell $(PKG_CONFIG) --libs $(COMPOSITOR_PKGS) 2>/dev/null)

.PHONY: all clean test compositor

all: test-window-model

test-window-model: window_model.c test_window_model.c window_model.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 -o $@ window_model.c test_window_model.c

test: test-window-model
	./test-window-model

compositor: josh-compositor

josh-compositor: josh_compositor.c window_model.c window_model.h
	@$(PKG_CONFIG) --exists $(COMPOSITOR_PKGS) || { \
		echo "Missing compositor build dependencies: $(COMPOSITOR_PKGS)" >&2; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(COMPOSITOR_CFLAGS) -std=c11 \
		-o $@ josh_compositor.c window_model.c $(COMPOSITOR_LIBS)

clean:
	rm -f test-window-model josh-compositor
