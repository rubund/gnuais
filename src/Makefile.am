
bin_PROGRAMS = gnuais gnuaissavesoundtofile

gnuais_SOURCES = 				\
	ais.c ais.h 				\
	input.c input.h 			\
	receiver.c receiver.h 			\
	protodec.c protodec.h 			\
	serial.c serial.h 			\
	out_mysql.c out_mysql.h 		\
	out_json.c out_json.h 			\
	cache.c cache.h 			\
	cfgfile.c cfgfile.h 			\
	hmalloc.c hmalloc.h 			\
	cfg.c cfg.h 				\
	hlog.c hlog.h 				\
	rwlock.c rwlock.h 			\
	splay.c splay.h 			\
	spsymbol.c 				\
	crc32.c crc32.h 			\
	filter.c filter.h filter-i386.h

gnuais_LDFLAGS = -pthread -lm -lasound @mysqleller@ @curleller@

gnuaissavesoundtofile_SOURCES = savetofile.c input.c \
	hmalloc.c hlog.c rwlock.c cfg.c cfgfile.c
gnuaissavesoundtofile_LDFLAGS = -pthread -lm -lasound 

SUBDIRS = @buildgui@
DIST_SUBDIRS = gui

