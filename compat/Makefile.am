pkglib_LTLIBRARIES = libcompat.la
libcompat_la_SOURCES = dummy.c
libcompat_la_LIBADD = $(LTLIBOBJS) $(LIBRT)
libcompat_la_LDFLAGS = -no-undefined -static

BUILT_SOURCES = dummy.c
CLEANFILES = dummy.c
AUTOMAKE_OPTIONS = subdir-objects

dummy.c:
	$(AM_V_at)rm -f dummy.c
	$(AM_V_GEN)echo '/* Automatically generated */' > dummy.c


check_PROGRAMS = \
	test_strnstr

TESTS = $(check_PROGRAMS)

test_strnstr_SOURCES = test/strnstr.c
test_strnstr_LDADD = libcompat.la
