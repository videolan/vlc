noinst_HEADERS = stdbit/stdbit.h stdckdint/stdckdint.h
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
	test_strnstr \
	test_ckd_ckd \
	test_ckd_builtin \
	test_ckd_compat \
	$(NULL)

AM_TESTS_ENVIRONMENT = ASAN_OPTIONS="allocator_may_return_null=1"

test_heap_SOURCES = test/heap.c
test_heap_LDADD = libcompat.la
test_strnstr_SOURCES = test/strnstr.c
test_strnstr_LDADD = libcompat.la

test_ckd_ckd_SOURCES = test/ckd.c
test_ckd_ckd_CFLAGS = -DTEST_CKD
if HAVE_STDCKDINT
test_ckd_ckd_CFLAGS += -DHAVE_CKD
endif
test_ckd_builtin_SOURCES = $(test_ckd_ckd_SOURCES)
test_ckd_builtin_CFLAGS = -DTEST_BUILTIN
test_ckd_compat_SOURCES = $(test_ckd_ckd_SOURCES)
test_ckd_compat_CFLAGS = -DTEST_COMPAT

if HAVE_DARWIN
check_PROGRAMS += test_clock_nanosleep

test_clock_nanosleep_SOURCES = test/clock_nanosleep.c
test_clock_nanosleep_LDADD = libcompat.la
endif

TESTS = $(check_PROGRAMS)
