libhttps_plugin_la_SOURCES = access/http/access.c
libhttps_plugin_la_LIBADD = libvlc_http.la
access_LTLIBRARIES += libhttps_plugin.la

libvlc_http_la_SOURCES = \
	access/http/transport.h \
	access/http/message.c access/http/message.h \
	access/http/resource.c access/http/resource.h \
	access/http/file.c access/http/file.h \
	access/http/live.c access/http/live.h \
	access/http/hpack.c access/http/hpack.h access/http/hpackenc.c \
	access/http/h2frame.c access/http/h2frame.h \
	access/http/h2output.c access/http/h2output.h \
	access/http/h2conn.c access/http/h1conn.c \
	access/http/chunked.c access/http/tunnel.c access/http/conn.h \
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
h2output_test_SOURCES = access/http/h2output_test.c
h2output_test_LDADD = libvlc_http.la $(LIBPTHREAD)
h2conn_test_SOURCES = access/http/h2conn_test.c
h2conn_test_LDADD = libvlc_http.la $(LIBPTHREAD)
h1conn_test_SOURCES = access/http/h1conn_test.c
h1conn_test_LDADD = libvlc_http.la
h1chunked_test_SOURCES = access/http/chunked_test.c
h1chunked_test_LDADD = libvlc_http.la
http_msg_test_SOURCES = access/http/message_test.c \
	access/http/message.c access/http/message.h
http_file_test_SOURCES = access/http/file_test.c \
	access/http/message.c access/http/message.h \
	access/http/resource.c access/http/resource.h \
	access/http/file.c access/http/file.h
http_tunnel_test_SOURCES = access/http/tunnel_test.c
http_tunnel_test_LDADD = libvlc_http.la
check_PROGRAMS += hpack_test hpackenc_test \
	h2frame_test h2output_test h2conn_test h1conn_test h1chunked_test \
	http_msg_test http_file_test http_tunnel_test
TESTS += hpack_test hpackenc_test \
	h2frame_test h2output_test h2conn_test h1conn_test h1chunked_test \
	http_msg_test http_file_test http_tunnel_test
