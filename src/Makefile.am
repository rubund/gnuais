
bin_PROGRAMS = gnuais gnuaissavesoundtofile

gnuais_SOURCES = ais.c input.c receiver.c protodec.c \
	serial.c out_mysql.c out_json.c cache.c \
	cfgfile.c hmalloc.c cfg.c hlog.c rwlock.c \
	splay.c spsymbol.c crc32.c \
	filter.c filter.h
gnuais_LDFLAGS = -pthread -lm -lasound @mysqleller@ @curleller@

gnuaissavesoundtofile_SOURCES = savetofile.c input.c \
	hmalloc.c hlog.c rwlock.c cfg.c cfgfile.c
gnuaissavesoundtofile_LDFLAGS = -pthread -lm -lasound 

EXTRA_DIST = input.h signalin.h protodec.h ais.h \
	serial.h out_mysql.h out_json.h cache.h \
	cfgfile.h hmalloc.h cfg.h hlog.h \
	rwlock.h splay.h crc32.h

SUBDIRS = @buildgui@
DIST_SUBDIRS = gui

