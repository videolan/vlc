# Unit/regression tests
#
check_PROGRAMS = \
	test_block \
	test_dictionary \
	test_i18n_atof \
	test_url \
	test_utf8 \
	test_xmlent \
	test_headers

TESTS = $(check_PROGRAMS)

AM_CFLAGS = `$(VLC_CONFIG) --cflags libvlc`
AM_LDFLAGS = -no-install
LDADD = ../libvlccore.la

test_block_SOURCES = test_block.c ../misc/block.c
test_dictionary_SOURCES = dictionary.c
test_i18n_atof_SOURCES = i18n_atof.c
test_url_SOURCES = url.c
test_utf8_SOURCES = utf8.c
test_xmlent_SOURCES = xmlent.c
test_headers_SOURCES = headers.c
