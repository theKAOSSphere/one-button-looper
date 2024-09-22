#!/usr/bin/make -f
# Makefile for kaoss_obl.lv2 #
# ----------------------- #
# Created by KAOSS
#

include Makefile.mk

# --------------------------------------------------------------

PREFIX  ?= /usr/local
DESTDIR ?=

# --------------------------------------------------------------
# Default target is to build all plugins

all: build

# --------------------------------------------------------------
# obl build rules

build: obl.lv2/obl$(LIB_EXT) obl.lv2/manifest.ttl

obl.lv2/obl$(LIB_EXT): obl.cpp
	$(CXX) $^ $(BUILD_CXX_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

obl.lv2/manifest.ttl: obl.lv2/manifest.ttl.in
	sed -e "s|@LIB_EXT@|$(LIB_EXT)|" $< > $@

# --------------------------------------------------------------

clean:
	rm -f obl.lv2/obl$(LIB_EXT) obl.lv2/manifest.ttl

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/obl.lv2

	install -m 644 obl.lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/obl.lv2/
	install -m 644 obl.lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/obl.lv2/
	cp -r obl.lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/obl.lv2/

# --------------------------------------------------------------
