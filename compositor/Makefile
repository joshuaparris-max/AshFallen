CC ?= cc
PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner
CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
WLROOTS_PKG ?= wlroots-0.20
COMPOSITOR_PKGS := $(WLROOTS_PKG) wayland-server xkbcommon
CLIENT_PKGS := wayland-client
COMPOSITOR_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(COMPOSITOR_PKGS) 2>/dev/null)
COMPOSITOR_LIBS := $(shell $(PKG_CONFIG) --libs $(COMPOSITOR_PKGS) 2>/dev/null)
CLIENT_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(CLIENT_PKGS) 2>/dev/null)
CLIENT_LIBS := $(shell $(PKG_CONFIG) --libs $(CLIENT_PKGS) 2>/dev/null)
WAYLAND_PROTOCOLS_DIR := $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols 2>/dev/null)
XDG_SHELL_XML := $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

.PHONY: all clean test compositor

all: test-window-model

test-window-model: window_model.c test_window_model.c window_model.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -std=c11 -o $@ window_model.c test_window_model.c

test: test-window-model
	./test-window-model

compositor: josh-compositor test-xdg-client

josh-compositor: josh_compositor.c window_model.c window_model.h
	@$(PKG_CONFIG) --exists $(COMPOSITOR_PKGS) || { \
		echo "Missing compositor build dependencies: $(COMPOSITOR_PKGS)" >&2; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(COMPOSITOR_CFLAGS) -std=c11 \
		-o $@ josh_compositor.c window_model.c $(COMPOSITOR_LIBS)

xdg-shell-client-protocol.h: $(XDG_SHELL_XML)
	@test -f "$(XDG_SHELL_XML)" || { echo "xdg-shell protocol XML not found" >&2; exit 1; }
	$(WAYLAND_SCANNER) client-header "$<" "$@"

xdg-shell-protocol.c: $(XDG_SHELL_XML)
	@test -f "$(XDG_SHELL_XML)" || { echo "xdg-shell protocol XML not found" >&2; exit 1; }
	$(WAYLAND_SCANNER) private-code "$<" "$@"

test-xdg-client: test_xdg_client.c xdg-shell-client-protocol.h xdg-shell-protocol.c
	@$(PKG_CONFIG) --exists $(CLIENT_PKGS) wayland-protocols || { \
		echo "Missing Wayland client build dependencies" >&2; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(CLIENT_CFLAGS) -std=c11 \
		-o $@ test_xdg_client.c xdg-shell-protocol.c $(CLIENT_LIBS)

clean:
	rm -f test-window-model josh-compositor test-xdg-client \
		xdg-shell-client-protocol.h xdg-shell-protocol.c
