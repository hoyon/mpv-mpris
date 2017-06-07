CFLAGS=-std=c99 -Wall -Wextra -O2 `pkg-config --cflags gio-2.0 gio-unix-2.0 mpv`
LDFLAGS=`pkg-config --libs gio-2.0 gio-unix-2.0`

mpris.so: mpris.c
	gcc mpris.c -o mpris.so $(CFLAGS) $(LDFLAGS) -shared -fPIC

install: mpris.so
	mkdir -p $(HOME)/.config/mpv/scripts
	cp mpris.so -t $(HOME)/.config/mpv/scripts

clean:
	rm mpris.so
