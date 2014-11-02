cmake_minimum_required (VERSION 2.6)

SET(CMAKE_BUILD_TYPE Release CACHE STRING "If build type is not specified on command line, build Release")

project (gnuais)

INCLUDE(CheckIncludeFiles)

CHECK_INCLUDE_FILES(alsa/asoundlib.h HAVE_ALSA)
CHECK_INCLUDE_FILES(mysql/mysql.h HAVE_MYSQL)
CHECK_INCLUDE_FILES(curl/curl.h HAVE_CURL)
CHECK_INCLUDE_FILES(pulse/pulseaudio.h HAVE_PULSEAUDIO)

if(NOT HAVE_ALSA)
	MESSAGE(FATAL_ERROR "The development files for ALSA are required - libasound-dev")
endif(NOT HAVE_ALSA)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(FILES gnuais.conf-example DESTINATION share/doc/gnuais)

add_subdirectory(src)
