hpack_test_SOURCES = access/http/hpack.c access/http/hpack.h
hpack_test_CFLAGS = -DDEC_TEST
hpackenc_test_SOURCES = $(hpack_test_SOURCES) access/http/hpackenc.c
hpackenc_test_CFLAGS = -DENC_TEST
h2frame_test_SOURCES = access/http/h2frame_test.c \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h
check_PROGRAMS += hpack_test hpackenc_test h2frame_test
TESTS += hpack_test hpackenc_test h2frame_test
