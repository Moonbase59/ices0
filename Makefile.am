## Process this with automake to create Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src conf doc

EXTRA_DIST = README BUGS AUTHORS TODO INSTALL COPYING NEWS README.playlist

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
