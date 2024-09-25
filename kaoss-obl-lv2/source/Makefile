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
# kaoss_obl build rules

build: kaoss_obl.lv2/kaoss_obl$(LIB_EXT) kaoss_obl.lv2/manifest.ttl

kaoss_obl.lv2/kaoss_obl$(LIB_EXT): obl.cpp
	$(CXX) $^ $(BUILD_CXX_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

kaoss_obl.lv2/manifest.ttl: kaoss_obl.lv2/manifest.ttl
	sed -e "s|@LIB_EXT@|$(LIB_EXT)|" $< > $@

# --------------------------------------------------------------

clean:
	rm -f kaoss_obl.lv2/kaoss_obl$(LIB_EXT) kaoss_obl.lv2/manifest.ttl

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/kaoss_obl.lv2

	install -m 644 kaoss_obl.lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/kaoss_obl.lv2/
	install -m 644 kaoss_obl.lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/kaoss_obl.lv2/
	cp -r kaoss_obl.lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/kaoss_obl.lv2/

# --------------------------------------------------------------
