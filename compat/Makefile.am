pkglib_LTLIBRARIES = libcompat.la
libcompat_la_SOURCES = dummy.c
libcompat_la_LIBADD = $(LTLIBOBJS)
libcompat_la_LDFLAGS = -no-undefined -static

BUILT_SOURCES = dummy.c
CLEANFILES = dummy.c

dummy.c:
	$(AM_V_at)rm -f dummy.c
	$(AM_V_GEN)echo '/* Automatically generated */' > dummy.c
