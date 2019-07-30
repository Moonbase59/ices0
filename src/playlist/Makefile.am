## Process this with automake to create Makefile.in

INCLUDES = -DICES_MODULEDIR=\"$(moddir)\" -I$(top_srcdir)/src

noinst_LIBRARIES = libplaylist.a
noinst_HEADERS = playlist.h pm_builtin.h pm_script.h rand.h

libplaylist_a_SOURCES = playlist.c pm_builtin.c pm_script.c rand.c
EXTRA_libplaylist_a_SOURCES = pm_python.c pm_perl.c

libplaylist_a_LIBADD = $(PLAYLIST_OBJECTS)
libplaylist_a_DEPENDENCIES = $(libplaylist_a_LIBADD)
