PKG_CONFIG = pkg-config

INSTALL := install
MKDIR := mkdir
RMDIR := rmdir
LN := ln
RM := rm

CFLAGS += -std=c99 -Wall -Wextra -O2 `$(PKG_CONFIG) --cflags gio-2.0 gio-unix-2.0 glib-2.0 mpv`
LDFLAGS += `$(PKG_CONFIG) --libs gio-2.0 gio-unix-2.0 glib-2.0`

SCRIPTS_DIR := $(HOME)/.config/mpv/scripts

PREFIX := /usr/local
PLUGINDIR := $(PREFIX)/lib/mpv-mpris
SYS_SCRIPTS_DIR := /etc/mpv/scripts

.PHONY: \
  install install-user install-system \
  uninstall uninstall-user uninstall-system \
  clean

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(CFLAGS) $(LDFLAGS) -shared -fPIC

ifneq ($(shell id -u),0)
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

clean:
	rm -f mpris.so
