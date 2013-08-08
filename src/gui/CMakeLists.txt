cmake_minimum_required (VERSION 2.6)
project(gnuais)

add_executable(gnuaisgui gui.c) 

find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0)
pkg_check_modules(GTHREAD gthread-2.0)
pkg_check_modules(OSMGPSMAP osmgpsmap)


if (NOT GTK2_FOUND)
	MESSAGE(FATAL_ERROR "libgtk-2.0 is required")
endif (NOT GTK2_FOUND)

if (NOT GTHREAD_FOUND)
	MESSAGE(FATAL_ERROR "libgthread-2.0 is required")
endif (NOT GTHREAD_FOUND)

if (NOT OSMGPSMAP_FOUND)
	MESSAGE(FATAL_ERROR "libosmgpsmap-dev is required")
endif (NOT OSMGPSMAP_FOUND)


target_link_libraries(gnuaisgui ${GTK2_LIBRARIES})
target_link_libraries(gnuaisgui ${GTHREAD_LIBRARIES})
target_link_libraries(gnuaisgui osmgpsmap)

include_directories(${GTK2_INCLUDE_DIRS} /usr/include/osmgpsmap)

install(TARGETS gnuaisgui DESTINATION bin)


