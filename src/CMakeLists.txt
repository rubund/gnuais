if(HAVE_PULSEAUDIO)
        add_executable(gnuais ais.c ais.c cfg.c crc32.c hlog.c input.c out_mysql.c receiver.c splay.c cache.c cfgfile.c filter.c hmalloc.c out_json.c protodec.c rwlock.c serial.c spsymbol.c pulseaudio.c) 
else()
        add_executable(gnuais ais.c ais.c cfg.c crc32.c hlog.c input.c out_mysql.c receiver.c splay.c cache.c cfgfile.c filter.c hmalloc.c out_json.c protodec.c rwlock.c serial.c spsymbol.c) 
endif()

target_link_libraries(gnuais pthread asound)

if(HAVE_MYSQL)
    target_link_libraries(gnuais mysqlclient)
else() 
    message(WARNING "You do not have development files for mysql. Mysql functionality will therefore be disabled")
endif()

if(HAVE_CURL)
    target_link_libraries(gnuais curl)
else()
    message(WARNING "You do not have the development files for curl. Functionaly using curl will therefore be disabled")
endif()

if(HAVE_PULSEAUDIO)
    target_link_libraries(gnuais pulse-simple)
else()
    message(WARNING "You do not have the development files for pulseaudio. Support for pulseaudio will therefore not be included")
endif()

install(TARGETS gnuais DESTINATION bin)
install (FILES gnuais.conf DESTINATION /etc)

if(HAVE_GTK)
    add_subdirectory(gui)
endif(HAVE_GTK)
