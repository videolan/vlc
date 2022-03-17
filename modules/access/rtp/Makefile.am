rtpparse_LTLIBRARIES =
rtpparse_PLUGINS =
rtpparse_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(rtpparse_PLUGINS)
else
rtpparse_LTLIBRARIES += $(rtpparse_PLUGINS)
rtpparse_RPATH += -rpath '$(rtpparsedir)'
endif

# RTP plugin
access_PLUGINS += librtp_plugin.la
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

noinst_HEADERS += access/rtp/fmtp.h

# RTP payload parser plugins
rtpparsedir = $(accessdir)/rtp
rtpparse_PLUGINS += \
	librtp_ac3_plugin.la \
	librtp_mpeg12_plugin.la \
	librtp_pcm_plugin.la \
	librtp_raw_plugin.la \
	librtp_h264_plugin.la \
	librtp_opus_plugin.la \
	librtp_xiph_plugin.la

librtp_ac3_plugin_la_SOURCES = access/rtp/ac3.c
librtp_mpeg12_plugin_la_SOURCES = access/rtp/mpeg12.c

librtp_pcm_plugin_la_SOURCES = access/rtp/pcm.c
librtp_pcm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/rtp

librtp_raw_plugin_la_SOURCES = access/rtp/raw.c

librtp_h264_plugin_la_SOURCES = access/rtp/h264.c

librtp_opus_plugin_la_SOURCES = access/rtp/opus.c

librtp_xiph_plugin_la_SOURCES = access/rtp/xiph.c
