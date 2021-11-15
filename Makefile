PKG_CONFIG = pkg-config

INSTALL := install
MKDIR := mkdir

CFLAGS += -std=c99 -Wall -Wextra -O2 `$(PKG_CONFIG) --cflags gio-2.0 gio-unix-2.0 glib-2.0 mpv`
LDFLAGS += `$(PKG_CONFIG) --libs gio-2.0 gio-unix-2.0 glib-2.0`

SCRIPTS_DIR := $(HOME)/.config/mpv/scripts

.PHONY: \
  install \
  clean

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(CFLAGS) $(LDFLAGS) -shared -fPIC

install: mpris.so
	$(MKDIR) -p $(SCRIPTS_DIR)
	$(INSTALL) -t $(SCRIPTS_DIR) mpris.so

clean:
	rm -f mpris.so
