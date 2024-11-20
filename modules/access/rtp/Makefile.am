# RTP library
libvlc_rtp_la_SOURCES = access/rtp/rtpfmt.c access/rtp/rtp.h \
			access/rtp/session.c
libvlc_rtp_la_CPPFLAGS = -I$(srcdir)/access/rtp
libvlc_rtp_la_LDFLAGS = -no-undefined
noinst_LTLIBRARIES += libvlc_rtp.la

# RTP plugin
access_LTLIBRARIES += librtp_plugin.la
librtp_plugin_la_SOURCES = \
	access/rtp/input.c access/rtp/input.h \
	access/rtp/sdp.c access/rtp/sdp.h \
	access/rtp/datagram.c access/rtp/vlc_dtls.h \
	access/rtp/rtp.c access/rtp/rtp.h
librtp_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/rtp
librtp_plugin_la_CFLAGS = $(AM_CFLAGS)
librtp_plugin_la_LIBADD = libvlc_rtp.la $(SOCKET_LIBS)

# Secure RTP library
libvlc_srtp_la_SOURCES = access/rtp/srtp.c access/rtp/srtp.h
libvlc_srtp_la_CPPFLAGS = -I$(srcdir)/access/rtp
libvlc_srtp_la_CFLAGS = $(GCRYPT_CFLAGS)
libvlc_srtp_la_LDFLAGS = -static
libvlc_srtp_la_LIBADD = $(GCRYPT_LIBS)

if HAVE_GCRYPT
noinst_LTLIBRARIES += libvlc_srtp.la

librtp_plugin_la_CPPFLAGS += -DHAVE_SRTP
librtp_plugin_la_CFLAGS += $(GCRYPT_CFLAGS)
librtp_plugin_la_LIBADD += libvlc_srtp.la $(GCRYPT_LIBS)
endif

noinst_HEADERS += access/rtp/fmtp.h

# RTP payload parser plugins
rtpparsedir = $(accessdir)/rtp
rtpparse_LTLIBRARIES = \
	librtp_ac3_plugin.la \
	librtp_mpeg12_plugin.la \
	librtp_mpeg4_plugin.la \
	librtp_pcm_plugin.la \
	librtp_raw_plugin.la \
	librtp_h264_plugin.la \
	librtp_h265_plugin.la \
	librtp_opus_plugin.la \
	librtp_xiph_plugin.la

librtp_ac3_plugin_la_SOURCES = access/rtp/ac3.c
librtp_mpeg12_plugin_la_SOURCES = access/rtp/mpeg12.c

librtp_mpeg4_plugin_la_SOURCES = access/rtp/mpeg4.c

librtp_pcm_plugin_la_SOURCES = access/rtp/pcm.c
librtp_pcm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/rtp

librtp_raw_plugin_la_SOURCES = access/rtp/raw.c

librtp_h264_plugin_la_SOURCES = access/rtp/h264.c access/rtp/h26x.h

librtp_h265_plugin_la_SOURCES = access/rtp/h265.c access/rtp/h26x.h

librtp_opus_plugin_la_SOURCES = access/rtp/opus.c

librtp_xiph_plugin_la_SOURCES = access/rtp/xiph.c
