
bin_PROGRAMS = gnuaisgui

gnuaisgui_SOURCES = gui.c

gnuaisgui_LDFLAGS = $(shell pkg-config gtk+-2.0 --libs) $(shell pkg-config gthread-2.0 --libs) -lm

gnuaisgui_CFLAGS = $(shell pkg-config gtk+-2.0 --cflags)

EXTRA_DIST =  map.png map.cbr waterford.png waterford.cbr waterford2.cbr  waterford2.png README

