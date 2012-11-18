cmake_minimum_required (VERSION 2.6)
add_executable(gnuaisgui gui.c) 

find_package(PkgConfig)

pkg_check_modules(GTK2 gtk+-2.0)
pkg_check_modules(GTHREAD gthread-2.0)

if (NOT GTK2_FOUND)
	MESSAGE(FATAL_ERROR "libgtk-2.0 is required")
endif (NOT GTK2_FOUND)

if (NOT GTHREAD_FOUND)
	MESSAGE(FATAL_ERROR "libgthread-2.0 is required")
endif (NOT GTHREAD_FOUND)

target_link_libraries(gnuaisgui ${GTK2_LIBRARIES})
target_link_libraries(gnuaisgui ${GTHREAD_LIBRARIES})

include_directories(${GTK2_INCLUDE_DIRS})

install(TARGETS gnuaisgui DESTINATION bin)


