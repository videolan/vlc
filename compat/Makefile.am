pkglib_LTLIBRARIES = libcompat.la
libcompat_la_SOURCES = dummy.c
libcompat_la_LIBADD = $(LTLIBOBJS) $(LIBRT) $(LIBM)
libcompat_la_LDFLAGS = -no-undefined -static

AUTOMAKE_OPTIONS = subdir-objects

dummy.c:
	$(AM_V_at)rm -f dummy.c
	$(AM_V_GEN)echo '/* Automatically generated */' > dummy.c


check_PROGRAMS = \
	test_heap \
	test_strnstr

AM_TESTS_ENVIRONMENT = ASAN_OPTIONS="allocator_may_return_null=1"

test_heap_SOURCES = test/heap.c
test_heap_LDADD = libcompat.la
test_strnstr_SOURCES = test/strnstr.c
test_strnstr_LDADD = libcompat.la

if HAVE_DARWIN
check_PROGRAMS += test_clock_nanosleep

test_clock_nanosleep_SOURCES = test/clock_nanosleep.c
test_clock_nanosleep_LDADD = libcompat.la
endif

TESTS = $(check_PROGRAMS)
