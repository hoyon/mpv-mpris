PKG_CONFIG = pkg-config

INSTALL := install
MKDIR := mkdir
RMDIR := rmdir
LN := ln
RM := rm

COMMON_CFLAGS = -std=c99 -Wall -Wextra -O2 -pedantic
GLIB_DEPS = gio-2.0 gio-unix-2.0 glib-2.0

# Embedded cover art needs the complex property C API, which arrived in TagLib
# 2.0. Distributions still shipping 1.x build fine, just without that feature.
TAGLIB_MIN_VERSION = 2.0
USE_TAGLIB ?= $(shell $(PKG_CONFIG) --atleast-version=$(TAGLIB_MIN_VERSION) taglib_c && echo 1 || echo 0)

PKG_DEPS = $(GLIB_DEPS)
ifeq ($(USE_TAGLIB),1)
PKG_DEPS += taglib_c
TAGLIB_CFLAGS = -DHAVE_TAGLIB
endif

# Base flags, environment CFLAGS / LDFLAGS can be appended.
BASE_CFLAGS = $(COMMON_CFLAGS) $(TAGLIB_CFLAGS) $(shell $(PKG_CONFIG) --cflags $(PKG_DEPS) mpv)
BASE_LDFLAGS = $(shell $(PKG_CONFIG) --libs $(PKG_DEPS))

# Portable binary for GitHub releases: TagLib linked statically so the result
# depends only on libraries whose soname never moves. Point TAGLIB_PREFIX at a
# static TagLib >= 2.0 built with CMAKE_POSITION_INDEPENDENT_CODE=ON.
TAGLIB_PREFIX ?= $(CURDIR)/build/taglib

RELEASE_CFLAGS = $(COMMON_CFLAGS) -DHAVE_TAGLIB -fvisibility=hidden -fPIC \
  -I$(TAGLIB_PREFIX)/include/taglib \
  $(shell $(PKG_CONFIG) --cflags $(GLIB_DEPS) mpv)

# A private static libstdc++ must stay hidden, or another library in mpv's
# process can interpose on it. Nothing but the plugin entry point is exported.
RELEASE_LDFLAGS = \
  $(TAGLIB_PREFIX)/lib/libtag_c.a $(TAGLIB_PREFIX)/lib/libtag.a -lz \
  $(shell $(PKG_CONFIG) --libs $(GLIB_DEPS)) \
  -static-libstdc++ -static-libgcc \
  -Wl,--version-script=mpris.map

SCRIPTS_DIR := $(HOME)/.config/mpv/scripts

PREFIX := /usr/local
PLUGINDIR := $(PREFIX)/lib/mpv-mpris
SYS_SCRIPTS_DIR := /etc/mpv/scripts

UID ?= $(shell id -u)

.PHONY: \
  install install-user install-system \
  uninstall uninstall-user uninstall-system \
  release release-check \
  test test-taglib \
  clean

mpris.so: mpris.c
	$(CC) mpris.c -o mpris.so $(BASE_CFLAGS) $(CFLAGS) $(CPPFLAGS) $(BASE_LDFLAGS) $(LDFLAGS) -shared -fPIC

release: mpris.c mpris.map
	$(CC) -c mpris.c -o mpris.o $(RELEASE_CFLAGS) $(CPPFLAGS)
	$(CXX) mpris.o -o mpris.so -shared $(RELEASE_LDFLAGS)
	$(RM) -f mpris.o
	$(MAKE) release-check

release-check:
	@ldd mpris.so
	@if ldd mpris.so | grep -E 'libav|libtag|libstdc\+\+'; then \
	  echo "error: release binary has a version-unstable dynamic dependency"; \
	  exit 1; \
	fi
	@if nm -D --defined-only mpris.so | grep ' _Z'; then \
	  echo "error: release binary exports C++ symbols"; \
	  exit 1; \
	fi

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

test-taglib: mpris.so
	$(MAKE) -C test test-taglib

clean:
	rm -f mpris.so mpris.o
	rm -rf build
	$(MAKE) -C test clean
