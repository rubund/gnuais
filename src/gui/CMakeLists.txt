#cmake_minimum_required (VERSION 2.6)
#project(gnuais)


find_package(PkgConfig)

pkg_check_modules(GTK3 gtk+-3.0)
pkg_check_modules(GTHREAD gthread-2.0)
pkg_check_modules(OSMGPSMAP osmgpsmap-1.0>=1.0.0)


#if (NOT GTK3_FOUND)
#	MESSAGE(INFO, "libgtk-2.0 is required")
#endif (NOT GTK3_FOUND)
#
#if (NOT GTHREAD_FOUND)
#	MESSAGE(FATAL_ERROR "libgthread-2.0 is required")
#endif (NOT GTHREAD_FOUND)
#
#if (NOT OSMGPSMAP_FOUND)
#	MESSAGE(FATAL_ERROR "libosmgpsmap-dev is required")
#endif (NOT OSMGPSMAP_FOUND)

if (GTK3_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)
	add_executable(gnuaisgui gui.c osm-gps-map-ais.c) 

	target_link_libraries(gnuaisgui ${GTK3_LIBRARIES})
	target_link_libraries(gnuaisgui ${GTHREAD_LIBRARIES})
	target_link_libraries(gnuaisgui ${OSMGPSMAP_LIBRARIES})
	target_link_libraries(gnuaisgui m)

	target_link_libraries(gnuaisgui pthread)

	include_directories(${GTK3_INCLUDE_DIRS} ${OSMGPSMAP_INCLUDE_DIRS})

	install(TARGETS gnuaisgui DESTINATION bin)
else (GTK3_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)
	MESSAGE( "Missing one more packages for building gui. Skipping")
endif(GTK3_FOUND AND GTHREAD_FOUND AND OSMGPSMAP_FOUND)


