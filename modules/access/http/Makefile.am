hpack_test_SOURCES = access/http/hpack.c access/http/hpack.h
hpack_test_CFLAGS = -DDEC_TEST
hpackenc_test_SOURCES = $(hpack_test_SOURCES) access/http/hpackenc.c
hpackenc_test_CFLAGS = -DENC_TEST
h2frame_test_SOURCES = access/http/h2frame_test.c \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h
h2output_test_SOURCES = access/http/h2output_test.c \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h \
	access/http/h2output.c access/http/h2output.h
h2output_test_LDADD = $(LIBPTHREAD)
h2conn_test_SOURCES = access/http/h2conn_test.c \
	access/http/message.c access/http/message.h \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h \
	access/http/h2conn.c access/http/h2conn.h \
	access/http/h2output.c access/http/h2output.h
h2conn_test_LDADD = $(LIBPTHREAD)
http_msg_test_SOURCES = access/http/message_test.c \
	access/http/message.c access/http/message.h
http_file_test_SOURCES = access/http/file_test.c \
	access/http/message.c access/http/message.h \
	access/http/file.c access/http/file.h
check_PROGRAMS += hpack_test hpackenc_test \
	h2frame_test h2output_test h2conn_test \
	http_msg_test http_file_test
TESTS += hpack_test hpackenc_test \
	h2frame_test h2output_test h2conn_test \
	http_msg_test http_file_test
