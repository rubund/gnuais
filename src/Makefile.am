
bin_PROGRAMS = gnuais gnuaissavesoundtofile

gnuais_SOURCES = ais.c input.c signalin.c protodec.c \
	serial.c \
	cfgfile.c hmalloc.c cfg.c hlog.c rwlock.c
gnuais_LDFLAGS = -pthread -lm -lasound  @mysqleller@

gnuaissavesoundtofile_SOURCES = savetofile.c input.c hmalloc.c hlog.c rwlock.c
gnuaissavesoundtofile_LDFLAGS = -pthread -lm -lasound 

EXTRA_DIST = input.h signalin.h protodec.h ais.h \
	serial.h \
	cfgfile.h hmalloc.h cfg.h hlog.h rwlock.h

SUBDIRS = @buildgui@
DIST_SUBDIRS = gui

