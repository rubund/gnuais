#cmake_minimum_required (VERSION 2.6)
#project(gnuais)


find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0)
pkg_check_modules(GTHREAD gthread-2.0)
pkg_check_modules(OSMGPSMAP osmgpsmap>=0.7.3)


#if (NOT GTK2_FOUND)
#	MESSAGE(INFO, "libgtk-2.0 is required")
#endif (NOT GTK2_FOUND)
#
#if (NOT GTHREAD_FOUND)
#	MESSAGE(FATAL_ERROR "libgthread-2.0 is required")
#endif (NOT GTHREAD_FOUND)
#
#if (NOT OSMGPSMAP_FOUND)
#	MESSAGE(FATAL_ERROR "libosmgpsmap-dev is required")
#endif (NOT OSMGPSMAP_FOUND)

if (GTK2_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)
	add_executable(gnuaisgui gui.c osm-gps-map-ais.c) 

	target_link_libraries(gnuaisgui ${GTK2_LIBRARIES})
	target_link_libraries(gnuaisgui ${GTHREAD_LIBRARIES})
	target_link_libraries(gnuaisgui osmgpsmap)
	target_link_libraries(gnuaisgui m)

	target_link_libraries(gnuaisgui pthread)

	include_directories(${GTK2_INCLUDE_DIRS} /usr/include/osmgpsmap)

	install(TARGETS gnuaisgui DESTINATION bin)
else (GTK2_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)
	MESSAGE( "Missing one more packages for building gui. Skipping")
endif(GTK2_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)


