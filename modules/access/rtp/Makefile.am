# RTP plugin
access_LTLIBRARIES += librtp_plugin.la
librtp_plugin_la_SOURCES = \
	access/rtp/input.c \
	access/rtp/session.c \
	access/rtp/sdp.c access/rtp/sdp.h \
	access/rtp/rtpfmt.c \
	access/rtp/datagram.c access/rtp/vlc_dtls.h \
	access/rtp/rtp.c access/rtp/rtp.h
librtp_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/rtp
librtp_plugin_la_CFLAGS = $(AM_CFLAGS)
librtp_plugin_la_LIBADD = $(SOCKET_LIBS)

# Secure RTP library
libvlc_srtp_la_SOURCES = access/rtp/srtp.c access/rtp/srtp.h
libvlc_srtp_la_CPPFLAGS = -I$(srcdir)/access/rtp
libvlc_srtp_la_CFLAGS = $(GCRYPT_CFLAGS)
libvlc_srtp_la_LDFLAGS = -static
libvlc_srtp_la_LIBADD = $(GCRYPT_LIBS)

librtp_plugin_la_DEPENDENCIES =
if HAVE_GCRYPT
noinst_LTLIBRARIES += libvlc_srtp.la

librtp_plugin_la_CPPFLAGS += -DHAVE_SRTP
librtp_plugin_la_CFLAGS += $(GCRYPT_CFLAGS)
librtp_plugin_la_LIBADD += libvlc_srtp.la $(GCRYPT_LIBS)
librtp_plugin_la_DEPENDENCIES += libvlc_srtp.la
endif

# RTP payload parser plugins
rtpparsedir = $(accessdir)/rtp
rtpparse_LTLIBRARIES = \
	librtp_mpeg12_plugin.la \
	librtp_pcm_plugin.la

librtp_mpeg12_plugin_la_SOURCES = access/rtp/mpeg12.c

librtp_pcm_plugin_la_SOURCES = access/rtp/pcm.c
librtp_pcm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/rtp
