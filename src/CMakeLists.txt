add_executable(gnuais ais.c ais.c cfg.c crc32.c hlog.c input.c out_mysql.c receiver.c splay.c cache.c cfgfile.c filter.c hmalloc.c out_json.c protodec.c rwlock.c serial.c spsymbol.c) 

target_link_libraries(gnuais pthread m asound)

if(HAVE_MYSQL)
    target_link_libraries(gnuais mysqlclient)
endif(HAVE_MYSQL)

if(HAVE_CURL)
    target_link_libraries(gnuais curl)
endif(HAVE_CURL)

install(TARGETS gnuais DESTINATION bin)


