PKG_CONFIG = pkg-config

INSTALL := install

BASE_CFLAGS=-std=c99 -Wall -Wextra -O2 `$(PKG_CONFIG) --cflags gio-2.0 gio-unix-2.0 glib-2.0 mpv`
BASE_LDFLAGS=`$(PKG_CONFIG) --libs gio-2.0 gio-unix-2.0 glib-2.0`

SCRIPTS_DIR := $(HOME)/.config/mpv/scripts

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(BASE_CFLAGS) $(CFLAGS) $(BASE_LDFLAGS) $(LDFLAGS) -shared -fPIC

install: mpris.so
	$(INSTALL) -Dt $(SCRIPTS_DIR) mpris.so

clean:
	rm mpris.so
