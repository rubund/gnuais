
bin_PROGRAMS = gnuais gnuaisfromfile gnuaissavesoundtofile

gnuais_SOURCES = ais.c input.c signalin.c protodec.c
gnuais_LDFLAGS = -pthread -lm -lasound  @mysqleller@

gnuaisfromfile_SOURCES = aisfromfile.c signalin.c protodec.c
gnuaisfromfile_LDFLAGS = -pthread -lm  @mysqleller@

gnuaissavesoundtofile_SOURCES = savetofile.c input.c 
gnuaissavesoundtofile_LDFLAGS = -pthread -lm -lasound 
EXTRA_DIST = input.h signalin.h protodec.h ais.h

SUBDIRS = @buildgui@
DIST_SUBDIRS = gui

