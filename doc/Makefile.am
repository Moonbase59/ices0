## Process this with automake to create Makefile.in

AUTOMAKE_OPTIONS = foreign

doc_DATA = icesmanual.html

man_MANS = ices.1

edit = sed -e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@moddir\@,$(moddir),g' \
	-e 's/@VERSION\@/$(VERSION)/g' \
	-e 's,@PACKAGE\@,$(PACKAGE),g'

ices.1: $(srcdir)/ices.1.in
	$(edit) $(srcdir)/ices.1.in > ices.1

EXTRA_DIST = $(doc_DATA) ices.1.in
