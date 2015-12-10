libhttps_plugin_la_SOURCES = access/http/access.c
libhttps_plugin_la_LIBADD = libvlc_http.la
access_LTLIBRARIES += libhttps_plugin.la

libvlc_http_la_SOURCES = \
	access/http/transport.c access/http/transport.h \
	access/http/message.c access/http/message.h \
	access/http/file.c access/http/file.h \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h \
	access/http/h2output.c access/http/h2output.h \
	access/http/h2conn.c access/http/h2conn.h \
	access/http/h1conn.c access/http/h1conn.h access/http/chunked.c \
	access/http/connmgr.c access/http/connmgr.h
libvlc_http_la_CPPFLAGS = -Dneedsomethinghere
libvlc_http_la_LIBADD = \
	$(LTLIBVLCCORE) ../compat/libcompat.la \
	$(SOCKET_LIBS) $(LIBPTHREAD)
#libvlc_http_la_LDFLAGS = -no-undefined -export-symbols-regex ^vlc_http_
#pkglib_LTLIBRARIES += libvlc_http.la
libvlc_http_la_LDFLAGS = -static
noinst_LTLIBRARIES += libvlc_http.la

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
