noinst_HEADERS = stdckdint/stdckdint.h
pkglib_LTLIBRARIES = libcompat.la
libcompat_la_SOURCES = dummy.c
libcompat_la_LIBADD = $(LTLIBOBJS) $(LIBRT) $(LIBM)
libcompat_la_LDFLAGS = -no-undefined -static

BUILT_SOURCES = dummy.c
CLEANFILES = dummy.c
AUTOMAKE_OPTIONS = subdir-objects

dummy.c:
	$(AM_V_at)rm -f dummy.c
	$(AM_V_GEN)echo '/* Automatically generated */' > dummy.c


check_PROGRAMS = \
	test_strnstr \
	test_ckd_ckd \
	test_ckd_builtin \
	test_ckd_compat \
	$(NULL)

TESTS = $(check_PROGRAMS)

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
