noinst_LTLIBRARIES = libcompat.la
libcompat_la_SOURCES = dummy.c
libcompat_la_LIBADD = $(LTLIBOBJS)
libcompat_la_LDFLAGS = -no-undefined

BUILT_SOURCES = dummy.c
CLEANFILES = dummy.c

dummy.c:
	rm -f dummy.c
	echo '/* Automatically generated */' > dummy.c
