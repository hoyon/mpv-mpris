PKG_CONFIG = pkg-config

CFLAGS=-std=c99 -Wall -Wextra -O2 `$(PKG_CONFIG) --cflags gio-2.0 gio-unix-2.0 glib-2.0 mpv`
LDFLAGS=`$(PKG_CONFIG) --libs gio-2.0 gio-unix-2.0 glib-2.0`

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(CFLAGS) $(LDFLAGS) -shared -fPIC

install: mpris.so
	mkdir -p $(HOME)/.config/mpv/scripts
	cp mpris.so -t $(HOME)/.config/mpv/scripts

clean:
	rm mpris.so
