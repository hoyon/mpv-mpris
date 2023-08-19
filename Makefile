PKG_CONFIG = pkg-config

INSTALL := install
MKDIR := mkdir
RMDIR := rmdir
LN := ln
RM := rm

# Base flags, environment CFLAGS / LDFLAGS can be appended.
BASE_CFLAGS = -std=c99 -Wall -Wextra -O2 -pedantic $(shell $(PKG_CONFIG) --cflags gio-2.0 gio-unix-2.0 glib-2.0 mpv libavformat)
BASE_LDFLAGS = $(shell $(PKG_CONFIG) --libs gio-2.0 gio-unix-2.0 glib-2.0 libavformat)

SCRIPTS_DIR := $(HOME)/.config/mpv/scripts

PREFIX := /usr/local
PLUGINDIR := $(PREFIX)/lib/mpv-mpris
SYS_SCRIPTS_DIR := /etc/mpv/scripts

UID ?= $(shell id -u)

.PHONY: \
  install install-user install-system \
  uninstall uninstall-user uninstall-system \
  test \
  clean

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(BASE_CFLAGS) $(CFLAGS) $(CPPFLAGS) $(BASE_LDFLAGS) $(LDFLAGS) -shared -fPIC

ifneq ($(UID),0)
install: install-user
uninstall: uninstall-user
else
install: install-system
uninstall: uninstall-system
endif

install-user: mpris.so
	$(MKDIR) -p $(SCRIPTS_DIR)
	$(INSTALL) -t $(SCRIPTS_DIR) mpris.so

uninstall-user:
	$(RM) -f $(SCRIPTS_DIR)/mpris.so
	$(RMDIR) -p $(SCRIPTS_DIR)

install-system: mpris.so
	$(MKDIR) -p $(DESTDIR)$(PLUGINDIR)
	$(INSTALL) -t $(DESTDIR)$(PLUGINDIR) mpris.so
	$(MKDIR) -p $(DESTDIR)$(SYS_SCRIPTS_DIR)
	$(LN) -s $(PLUGINDIR)/mpris.so $(DESTDIR)$(SYS_SCRIPTS_DIR)

uninstall-system:
	$(RM) -f $(DESTDIR)$(SYS_SCRIPTS_DIR)/mpris.so
	$(RMDIR) -p $(DESTDIR)$(SYS_SCRIPTS_DIR)
	$(RM) -f $(DESTDIR)$(PLUGINDIR)/mpris.so
	$(RMDIR) -p $(DESTDIR)$(PLUGINDIR)

test: mpris.so
	$(MAKE) -C test

clean:
	rm -f mpris.so
	$(MAKE) -C test clean
