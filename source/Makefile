#!/usr/bin/make -f
# Makefile for kaoss-obl.lv2 #
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
# kaoss-obl build rules

build: kaoss-obl.lv2/kaoss-obl$(LIB_EXT) kaoss-obl.lv2/manifest.ttl

kaoss-obl.lv2/kaoss-obl$(LIB_EXT): obl.cpp
	$(CXX) $^ $(BUILD_CXX_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

kaoss-obl.lv2/manifest.ttl: kaoss-obl.lv2/manifest.ttl.in
	sed -e "s|@LIB_EXT@|$(LIB_EXT)|" $< > $@

# --------------------------------------------------------------

clean:
	rm -f kaoss-obl.lv2/kaoss-obl$(LIB_EXT) kaoss-obl.lv2/manifest.ttl

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/kaoss-obl.lv2

	install -m 755 kaoss-obl.lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/kaoss-obl.lv2/
	install -m 644 kaoss-obl.lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/kaoss-obl.lv2/
	cp -r kaoss-obl.lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/kaoss-obl.lv2/
# --------------------------------------------------------------
