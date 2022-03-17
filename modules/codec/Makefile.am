codecdir = $(pluginsdir)/codec
codec_LTLIBRARIES =
codec_PLUGINS =

codec_RPATH =

if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(codec_PLUGINS)
else
codec_LTLIBRARIES += $(codec_PLUGINS)
codec_RPATH += -rpath '$(codecdir)'
endif

AM_CPPFLAGS += -I$(srcdir)/codec

libddummy_plugin_la_SOURCES = codec/ddummy.c
codec_PLUGINS += libddummy_plugin.la

libedummy_plugin_la_SOURCES = codec/edummy.c
if ENABLE_SOUT
codec_PLUGINS += libedummy_plugin.la
endif


### Audio codecs ###
libspdif_plugin_la_SOURCES = codec/spdif.c
codec_PLUGINS += libspdif_plugin.la

liba52_plugin_la_SOURCES = codec/a52.c
liba52_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_a52)
liba52_plugin_la_LIBADD = $(LIBS_a52)
liba52_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_a52) $(codec_RPATH)
EXTRA_LTLIBRARIES += liba52_plugin.la
codec_PLUGINS += $(LTLIBa52)

libdca_plugin_la_SOURCES = codec/dca.c
libdca_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(DCA_CFLAGS)
libdca_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libdca_plugin_la_LIBADD = $(LIBM) $(DCA_LIBS)
EXTRA_LTLIBRARIES += libdca_plugin.la
codec_PLUGINS += $(LTLIBdca)

libadpcm_plugin_la_SOURCES = codec/adpcm.c
codec_PLUGINS += libadpcm_plugin.la

libaes3_plugin_la_SOURCES = codec/aes3.c
codec_PLUGINS += libaes3_plugin.la

libaraw_plugin_la_SOURCES = codec/araw.c
libaraw_plugin_la_LIBADD = $(LIBM)
codec_PLUGINS += libaraw_plugin.la

libfaad_plugin_la_SOURCES = codec/faad.c packetizer/mpeg4audio.h
libfaad_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CFLAGS_faad)
libfaad_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libfaad_plugin_la_LIBADD = $(LIBS_faad) $(LIBM)
EXTRA_LTLIBRARIES += libfaad_plugin.la
codec_PLUGINS += $(LTLIBfaad)

libg711_plugin_la_SOURCES = codec/g711.c
codec_PLUGINS += libg711_plugin.la

libfluidsynth_plugin_la_SOURCES = codec/fluidsynth.c
libfluidsynth_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DDATADIR=\"$(datadir)\"
libfluidsynth_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLUIDSYNTH_CFLAGS)
libfluidsynth_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libfluidsynth_plugin_la_LIBADD = $(FLUIDSYNTH_LIBS)
EXTRA_LTLIBRARIES += libfluidsynth_plugin.la
codec_PLUGINS += $(LTLIBfluidsynth)
if HAVE_DARWIN
libfluidsynth_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,CoreServices
endif

libaudiotoolboxmidi_plugin_la_SOURCES = codec/audiotoolbox_midi.c
libaudiotoolboxmidi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,CoreFoundation,-framework,AudioToolbox
if HAVE_DARWIN
codec_PLUGINS += libaudiotoolboxmidi_plugin.la
endif

liblpcm_plugin_la_SOURCES = codec/lpcm.c
codec_PLUGINS += liblpcm_plugin.la

libmad_plugin_la_SOURCES = codec/mad.c
libmad_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(MAD_CFLAGS)
libmad_plugin_la_LIBADD = $(AM_LIBADD) $(MAD_LIBS)
if HAVE_MAD
audio_filter_PLUGINS += libmad_plugin.la
endif

libmpg123_plugin_la_SOURCES = codec/mpg123.c
libmpg123_plugin_la_CFLAGS = $(AM_CFLAGS) $(MPG123_CFLAGS)
libmpg123_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libmpg123_plugin_la_LIBADD = $(MPG123_LIBS)
EXTRA_LTLIBRARIES += libmpg123_plugin.la
codec_PLUGINS += $(LTLIBmpg123)

libuleaddvaudio_plugin_la_SOURCES = codec/uleaddvaudio.c
codec_PLUGINS += libuleaddvaudio_plugin.la

### Video codecs ###

libcdg_plugin_la_SOURCES = codec/cdg.c
codec_PLUGINS += libcdg_plugin.la

libdavs2_plugin_la_SOURCES = codec/davs2.c
libdavs2_plugin_la_CFLAGS = $(AM_CFLAGS) $(DAVS2_CFLAGS)
libdavs2_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libdavs2_plugin_la_LIBADD = $(DAVS2_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libdavs2_plugin.la
codec_PLUGINS += $(LTLIBdavs2)

liblibmpeg2_plugin_la_SOURCES = codec/libmpeg2.c \
	codec/synchro.c codec/synchro.h
liblibmpeg2_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_libmpeg2)
liblibmpeg2_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
liblibmpeg2_plugin_la_LIBADD = $(LIBS_libmpeg2)
EXTRA_LTLIBRARIES += liblibmpeg2_plugin.la
codec_PLUGINS += $(LTLIBlibmpeg2)

librawvideo_plugin_la_SOURCES = codec/rawvideo.c
codec_PLUGINS += librawvideo_plugin.la

librtpvideo_plugin_la_SOURCES = codec/rtpvideo.c
if ENABLE_SOUT
codec_PLUGINS += librtpvideo_plugin.la
endif


libschroedinger_plugin_la_SOURCES = codec/schroedinger.c
libschroedinger_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_schroedinger)
libschroedinger_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libschroedinger_plugin_la_LIBADD = $(LIBS_schroedinger)
EXTRA_LTLIBRARIES += libschroedinger_plugin.la
codec_PLUGINS += $(LTLIBschroedinger)

librtp_rawvid_plugin_la_SOURCES = codec/rtp-rawvid.c
codec_PLUGINS += librtp_rawvid_plugin.la


### Image codecs ###

libpng_plugin_la_SOURCES = codec/png.c
libpng_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libpng_plugin_la_LIBADD = -lpng -lz $(LIBM)
EXTRA_LTLIBRARIES += libpng_plugin.la
codec_PLUGINS += $(LTLIBpng)

libjpeg_plugin_la_SOURCES = codec/jpeg.c
libjpeg_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libjpeg_plugin_la_LIBADD = -ljpeg
EXTRA_LTLIBRARIES += libjpeg_plugin.la
codec_PLUGINS += $(LTLIBjpeg)

libbpg_plugin_la_SOURCES = codec/bpg.c
libbpg_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libbpg_plugin_la_LIBADD = -lbpg
EXTRA_LTLIBRARIES += libbpg_plugin.la
codec_PLUGINS += $(LTLIBbpg)

libsvgdec_plugin_la_SOURCES = codec/svg.c
libsvgdec_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_svgdec)
libsvgdec_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_svg) $(codec_RPATH)
libsvgdec_plugin_la_LIBADD = $(LIBS_svgdec)
EXTRA_LTLIBRARIES += libsvgdec_plugin.la
codec_PLUGINS += $(LTLIBsvgdec)

libxwd_plugin_la_SOURCES = codec/xwd.c
libxwd_plugin_la_CFLAGS = $(AM_CFLAGS) $(XPROTO_CFLAGS)
libxwd_plugin_la_LIBADD = $(XPROTO_LIBS)
libxwd_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
if HAVE_XCB
codec_PLUGINS += libxwd_plugin.la
endif


### SPU ###

liblibass_plugin_la_SOURCES = codec/libass.c
liblibass_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_libass)
liblibass_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBASS_CFLAGS)
liblibass_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
liblibass_plugin_la_LIBADD = $(LIBASS_LIBS) $(LIBS_libass) $(LIBS_freetype) $(FREETYPE_LIBS)
if HAVE_WINSTORE
liblibass_plugin_la_LIBADD += -ldwrite
endif
EXTRA_LTLIBRARIES += liblibass_plugin.la
codec_PLUGINS += $(LTLIBlibass)

libcc_plugin_la_SOURCES = codec/cc.c codec/cc.h codec/substext.h \
                          codec/cea708.h codec/cea708.c
codec_PLUGINS += libcc_plugin.la

libcvdsub_plugin_la_SOURCES = codec/cvdsub.c demux/mpeg/timestamps.h
codec_PLUGINS += libcvdsub_plugin.la

libdvbsub_plugin_la_SOURCES = codec/dvbsub.c
codec_PLUGINS += libdvbsub_plugin.la

libaribcaption_plugin_la_SOURCES = codec/arib/libaribcaption.c
libaribcaption_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARIBCAPTION_CFLAGS)
libaribcaption_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libaribcaption_plugin_la_LIBADD = $(ARIBCAPTION_LIBS) $(LIBS_freetype) $(FREETYPE_LIBS)
EXTRA_LTLIBRARIES += libaribcaption_plugin.la
codec_PLUGINS += $(LTLIBaribcaption)

libaribsub_plugin_la_SOURCES = codec/arib/aribsub.c codec/arib/substext.h
libaribsub_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARIBB24_CFLAGS)
libaribsub_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libaribsub_plugin_la_LIBADD = $(ARIBB24_LIBS)
EXTRA_LTLIBRARIES += libaribsub_plugin.la
codec_PLUGINS += $(LTLIBaribsub)

libscte18_plugin_la_SOURCES = codec/scte18.c codec/scte18.h \
                              codec/atsc_a65.c codec/atsc_a65.h
codec_PLUGINS += libscte18_plugin.la

libscte27_plugin_la_SOURCES = codec/scte27.c
codec_PLUGINS += libscte27_plugin.la

libspudec_plugin_la_SOURCES = codec/spudec/spudec.c codec/spudec/parse.c codec/spudec/spudec.h
libspudec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
codec_PLUGINS += libspudec_plugin.la

libstl_plugin_la_SOURCES = codec/stl.c
codec_PLUGINS += libstl_plugin.la

libsubsdec_plugin_la_SOURCES = codec/subsdec.c codec/substext.h
codec_PLUGINS += libsubsdec_plugin.la

libsubsusf_plugin_la_SOURCES = codec/subsusf.c
codec_PLUGINS += libsubsusf_plugin.la

libttml_plugin_la_SOURCES = codec/ttml/substtml.c \
                            demux/ttml.c \
                            codec/ttml/ttml.h codec/ttml/ttml.c \
                            codec/ttml/imageupdater.h \
                            codec/ttml/ttmlpes.h
codec_PLUGINS += libttml_plugin.la

libwebvtt_plugin_la_SOURCES = codec/webvtt/subsvtt.c \
                              codec/webvtt/webvtt.c \
                              codec/webvtt/webvtt.h \
                              demux/webvtt.c \
                              demux/mp4/minibox.h
if ENABLE_SOUT
libwebvtt_plugin_la_SOURCES += codec/webvtt/encvtt.c
endif
codec_PLUGINS += libwebvtt_plugin.la
if ENABLE_CSS
# dependency: we need grammar build first
BUILT_SOURCES += codec/webvtt/CSSGrammar.h
libwebvtt_plugin_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/codec/webvtt/ -I$(builddir)/codec/webvtt/
libwebvtt_plugin_la_LFLAGS = $(AM_LFLAGS) --outfile=lex.yy.c
libwebvtt_plugin_la_SOURCES += codec/webvtt/CSSGrammar.y \
                               codec/webvtt/CSSLexer.l \
                               codec/webvtt/css_parser.c \
                               codec/webvtt/css_parser.h \
                               codec/webvtt/css_style.c \
                               codec/webvtt/css_style.h \
                               codec/webvtt/css_bridge.h
css_parser_test_CFLAGS = $(libwebvtt_plugin_la_CFLAGS)
css_parser_test_LFLAGS = $(AM_LFLAGS) --outfile=lex.yy.c
css_parser_test_SOURCES = codec/webvtt/css_test.c \
                          codec/webvtt/css_parser.c \
                          codec/webvtt/CSSGrammar.y \
                          codec/webvtt/CSSLexer.l
check_PROGRAMS += css_parser_test
TESTS += css_parser_test
endif

libsvcdsub_plugin_la_SOURCES = codec/svcdsub.c demux/mpeg/timestamps.h
codec_PLUGINS += libsvcdsub_plugin.la

libt140_plugin_la_SOURCES = codec/t140.c
if ENABLE_SOUT
codec_PLUGINS += libt140_plugin.la
endif

libtelx_plugin_la_SOURCES = codec/telx.c
libtelx_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
EXTRA_LTLIBRARIES += libtelx_plugin.la
codec_PLUGINS += $(LTLIBtelx)

libtextst_plugin_la_SOURCES = codec/textst.c demux/mpeg/timestamps.h
libtextst_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
codec_PLUGINS += libtextst_plugin.la

libzvbi_plugin_la_SOURCES = codec/zvbi.c
libzvbi_plugin_la_CFLAGS = $(AM_CFLAGS) $(ZVBI_CFLAGS) $(CFLAGS_zvbi)
libzvbi_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libzvbi_plugin_la_LIBADD = $(ZVBI_LIBS) $(SOCKET_LIBS)
EXTRA_LTLIBRARIES += libzvbi_plugin.la
codec_PLUGINS += $(LTLIBzvbi)

libsubstx3g_plugin_la_SOURCES = codec/substx3g.c codec/substext.h
codec_PLUGINS += libsubstx3g_plugin.la

### Xiph ###

libflac_plugin_la_SOURCES = codec/flac.c
libflac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLAC_CFLAGS)
libflac_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libflac_plugin_la_LIBADD = $(FLAC_LIBS)
EXTRA_LTLIBRARIES += libflac_plugin.la
codec_PLUGINS += $(LTLIBflac)

libkate_plugin_la_SOURCES = codec/kate.c
libkate_plugin_la_CFLAGS = $(AM_CFLAGS) $(TIGER_CFLAGS) $(KATE_CFLAGS)
libkate_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libkate_plugin_la_LIBADD = $(TIGER_LIBS) $(KATE_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libkate_plugin.la
codec_PLUGINS += $(LTLIBkate)

libopus_plugin_la_SOURCES = codec/opus.c codec/opus_header.c codec/opus_header.h
libopus_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPUS_CFLAGS)
libopus_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libopus_plugin_la_LIBADD = $(OPUS_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libopus_plugin.la
codec_PLUGINS += $(LTLIBopus)

libspeex_plugin_la_SOURCES = codec/speex.c
libspeex_plugin_la_CFLAGS = $(AM_CFLAGS) $(SPEEX_CFLAGS)
libspeex_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libspeex_plugin_la_LIBADD = $(SPEEX_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libspeex_plugin.la
codec_PLUGINS += $(LTLIBspeex)

libtheora_plugin_la_SOURCES = codec/theora.c
libtheora_plugin_la_CFLAGS = $(AM_CFLAGS) $(THEORA_CFLAGS)
libtheora_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libtheora_plugin_la_LIBADD = $(THEORA_LIBS)
EXTRA_LTLIBRARIES += libtheora_plugin.la
codec_PLUGINS += $(LTLIBtheora)

libdaala_plugin_la_SOURCES = codec/daala.c
libdaala_plugin_la_CFLAGS = $(AM_CFLAGS) $(DAALA_CFLAGS)
libdaala_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libdaala_plugin_la_LIBADD = $(DAALA_LIBS)
EXTRA_LTLIBRARIES += libdaala_plugin.la
codec_PLUGINS += $(LTLIBdaala)

libtremor_plugin_la_SOURCES = codec/vorbis.c
libtremor_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DPLUGIN_TREMOR
libtremor_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libtremor_plugin_la_LIBADD = -lvorbisdec -logg
EXTRA_LTLIBRARIES += libtremor_plugin.la
codec_PLUGINS += $(LTLIBtremor)

libvorbis_plugin_la_SOURCES = codec/vorbis.c
libvorbis_plugin_la_CFLAGS = $(AM_CFLAGS) $(VORBIS_CFLAGS)
libvorbis_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libvorbis_plugin_la_LIBADD = $(VORBIS_LIBS)
EXTRA_LTLIBRARIES += libvorbis_plugin.la
codec_PLUGINS += $(LTLIBvorbis)

liboggspots_plugin_la_SOURCES = codec/oggspots.c
liboggspots_plugin_la_CFLAGS = $(AM_CFLAGS) $(OGGSPOTS_CFLAGS)
liboggspots_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
liboggspots_plugin_la_LIBADD = $(OGGSPOTS_LIBS)
EXTRA_LTLIBRARIES += liboggspots_plugin.la
codec_PLUGINS += $(LTLIBoggspots)

libvideotoolbox_plugin_la_SOURCES = codec/videotoolbox.c
libvideotoolbox_plugin_la_LIBADD = libchroma_copy.la libvlc_hxxxhelper.la libvlc_vtutils.la
libvideotoolbox_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,CoreFoundation -Wl,-framework,VideoToolbox -Wl,-framework,CoreMedia -Wl,-framework,CoreVideo
if HAVE_DARWIN
codec_PLUGINS += libvideotoolbox_plugin.la
endif

### FFmpeg/libav ###
libavcodec_common_la_SOURCES = codec/avcodec/fourcc.c codec/avcodec/avcommon.h \
	codec/avcodec/chroma.c codec/avcodec/chroma.h \
	codec/avcodec/avcommon_compat.h
libavcodec_common_la_CFLAGS = $(AVCODEC_CFLAGS) $(AM_CFLAGS)
libavcodec_common_la_LDFLAGS = -static

libavcodec_plugin_la_SOURCES = \
	codec/avcodec/video.c \
	codec/avcodec/subtitle.c \
	codec/avcodec/audio.c \
	codec/avcodec/va.c codec/avcodec/va.h \
	codec/avcodec/avcodec.c codec/avcodec/avcodec.h
if ENABLE_SOUT
libavcodec_plugin_la_SOURCES += codec/avcodec/encoder.c
endif
libavcodec_plugin_la_CFLAGS = $(AVCODEC_CFLAGS) $(AM_CFLAGS)
libavcodec_plugin_la_LIBADD = $(AVCODEC_LIBS) $(LIBM) libavcodec_common.la
libavcodec_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(SYMBOLIC_LDFLAGS)

if MERGE_FFMPEG
libavcodec_plugin_la_SOURCES += demux/avformat/demux.c access/avio.c packetizer/avparser.c
libavcodec_plugin_la_CFLAGS += $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS)
libavcodec_plugin_la_LIBADD += $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(LIBM)
if ENABLE_SOUT
libavcodec_plugin_la_SOURCES += demux/avformat/mux.c
endif
libavcodec_plugin_la_CFLAGS += -DMERGE_FFMPEG
endif
if HAVE_DARWIN
libavcodec_plugin_la_LDFLAGS += -Wl,-framework,Foundation -Wl,-framework,Security,-framework,CoreFoundation
endif
if HAVE_AVCODEC
noinst_LTLIBRARIES += libavcodec_common.la
codec_PLUGINS += libavcodec_plugin.la
endif

### avcodec hardware acceleration ###

libvaapi_plugin_la_SOURCES = \
	codec/avcodec/vaapi.c hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h \
	codec/avcodec/va_surface.c codec/avcodec/va_surface.h
libvaapi_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvaapi_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVCODEC_CFLAGS)
libvaapi_plugin_la_LIBADD = $(LIBVA_LIBS) $(AVCODEC_LIBS)
if HAVE_AVCODEC
if HAVE_VAAPI
codec_PLUGINS += libvaapi_plugin.la
endif
endif

libd3d9_common_la_SOURCES = video_chroma/d3d9_fmt.c video_chroma/d3d9_fmt.h \
	video_chroma/dxgi_fmt.c video_chroma/dxgi_fmt.h
libd3d9_common_la_LDFLAGS = -static
if HAVE_WIN32_DESKTOP
noinst_LTLIBRARIES += libd3d9_common.la
endif

libdxva2_plugin_la_SOURCES = \
	codec/avcodec/dxva2.c codec/avcodec/directx_va.c codec/avcodec/directx_va.h \
	codec/avcodec/va_surface.c codec/avcodec/va_surface.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h \
	codec/avcodec/dxva_blocklist.c
libdxva2_plugin_la_LIBADD = libd3d9_common.la $(LIBCOM) -luuid
if !HAVE_WINSTORE
if HAVE_AVCODEC_DXVA2
codec_PLUGINS += libdxva2_plugin.la
endif
endif

libd3d11_common_la_SOURCES = video_chroma/d3d11_fmt.c video_chroma/d3d11_fmt.h \
	video_chroma/dxgi_fmt.c video_chroma/dxgi_fmt.h
libd3d11_common_la_LDFLAGS = -static
if HAVE_WIN32
noinst_LTLIBRARIES += libd3d11_common.la
endif

libd3d11va_plugin_la_SOURCES = \
	codec/avcodec/d3d11va.c codec/avcodec/directx_va.c codec/avcodec/directx_va.h \
        codec/avcodec/va_surface.c codec/avcodec/va_surface.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h \
	codec/avcodec/dxva_blocklist.c
libd3d11va_plugin_la_LIBADD = libd3d11_common.la $(LIBCOM) -luuid
if HAVE_WINSTORE
libd3d11va_plugin_la_LIBADD += -ld3d11
endif
if HAVE_AVCODEC_D3D11VA
codec_PLUGINS += libd3d11va_plugin.la
endif

### OpenMAX ###

noinst_HEADERS += \
	codec/omxil/OMX_Broadcom.h \
	codec/omxil/OMX_Component.h \
	codec/omxil/OMX_Core.h \
	codec/omxil/OMX_Image.h \
	codec/omxil/OMX_IVCommon.h \
	codec/omxil/OMX_Types.h \
	codec/omxil/OMX_Audio.h \
	codec/omxil/OMX_Index.h \
	codec/omxil/OMX_Other.h \
	codec/omxil/OMX_Video.h

libomxil_plugin_la_SOURCES = \
	codec/omxil/utils.c codec/omxil/omxil_utils.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h \
	codec/omxil/qcom.c codec/omxil/qcom.h \
	codec/omxil/omxil.c codec/omxil/omxil.h codec/omxil/omxil_core.c codec/omxil/omxil_core.h
libomxil_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/codec/omxil $(CFLAGS_omxil)
libomxil_plugin_la_LIBADD = libchroma_copy.la $(LIBDL)
libomxil_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)

libmediacodec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/codec/omxil
libmediacodec_plugin_la_SOURCES = codec/omxil/mediacodec.c codec/omxil/mediacodec.h \
	codec/omxil/mediacodec_jni.c codec/omxil/mediacodec_ndk.c codec/omxil/utils.c \
	codec/omxil/omxil_utils.h codec/omxil/qcom.c codec/omxil/qcom.h
libmediacodec_plugin_la_LIBADD = libchroma_copy.la libvlc_hxxxhelper.la libandroid_utils.la $(LIBDL)
codec_PLUGINS += $(LTLIBomxil)
EXTRA_LTLIBRARIES += libomxil_plugin.la
if HAVE_ANDROID
codec_PLUGINS += libmediacodec_plugin.la
endif


### X26x encoders ###

libx265_plugin_la_SOURCES = codec/x265.c
libx265_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libx265_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x265)
libx265_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x265) $(codec_RPATH)
libx265_plugin_la_LIBADD = $(LIBS_x265)
EXTRA_LTLIBRARIES += libx265_plugin.la
codec_PLUGINS += $(LTLIBx265)

libx262_plugin_la_SOURCES = codec/x264.c
libx262_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DPLUGIN_X262
libx262_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x262)
libx262_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x262) $(codec_RPATH)
libx262_plugin_la_LIBADD = $(LIBS_x262) $(LIBM)
EXTRA_LTLIBRARIES += libx262_plugin.la
codec_PLUGINS += $(LTLIBx262)

libx264_plugin_la_SOURCES = codec/x264.c
libx264_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_x264) -DPLUGIN_X264
libx264_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x264)
libx264_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x264) $(codec_RPATH)
libx264_plugin_la_LIBADD = $(LIBS_x264) $(LIBM)
EXTRA_LTLIBRARIES += libx264_plugin.la
codec_PLUGINS += $(LTLIBx264)

libx26410b_plugin_la_SOURCES = codec/x264.c
libx26410b_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DPLUGIN_X264_10B
libx26410b_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x26410b)
libx26410b_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x26410b) $(codec_RPATH)
libx26410b_plugin_la_LIBADD = $(LIBS_x26410b) $(LIBM)
EXTRA_LTLIBRARIES += libx26410b_plugin.la
codec_PLUGINS += $(LTLIBx26410b)


### Misc encoders ###

libfdkaac_plugin_la_SOURCES = codec/fdkaac.c
libfdkaac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FDKAAC_CFLAGS)
libfdkaac_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libfdkaac_plugin_la_LIBADD = $(FDKAAC_LIBS)
EXTRA_LTLIBRARIES += libfdkaac_plugin.la
codec_PLUGINS += $(LTLIBfdkaac)

libshine_plugin_la_SOURCES = codec/shine.c
libshine_plugin_la_CFLAGS = $(AM_CFLAGS) $(SHINE_CFLAGS)
libshine_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH) $(SHINE_LIBS)
EXTRA_LTLIBRARIES += libshine_plugin.la
codec_PLUGINS += $(LTLIBshine)

libvpx_plugin_la_SOURCES = codec/vpx.c
libvpx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvpx_plugin_la_CFLAGS = $(AM_CFLAGS) $(VPX_CFLAGS) $(CPPFLAGS_vpx)
libvpx_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libvpx_plugin_la_LIBADD = $(VPX_LIBS)
EXTRA_LTLIBRARIES += libvpx_plugin.la
codec_PLUGINS += $(LTLIBvpx)

libaom_plugin_la_SOURCES = codec/aom.c \
                           packetizer/iso_color_tables.h
libaom_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libaom_plugin_la_CFLAGS = $(AM_CFLAGS) $(AOM_CFLAGS) $(CPPFLAGS_aom)
libaom_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libaom_plugin_la_LIBADD = $(AOM_LIBS)
EXTRA_LTLIBRARIES += libaom_plugin.la
codec_PLUGINS += $(LTLIBaom)

librav1e_plugin_la_SOURCES = codec/rav1e.c
librav1e_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
librav1e_plugin_la_CFLAGS = $(AM_CFLAGS) $(RAV1E_CFLAGS)
librav1e_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
librav1e_plugin_la_LIBADD = $(RAV1E_LIBS)
EXTRA_LTLIBRARIES += librav1e_plugin.la
codec_PLUGINS += $(LTLIBrav1e)

libtwolame_plugin_la_SOURCES = codec/twolame.c
libtwolame_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DLIBTWOLAME_STATIC
libtwolame_plugin_la_CFLAGS = $(AM_CFLAGS) $(TWOLAME_CFLAGS)
libtwolame_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libtwolame_plugin_la_LIBADD = $(TWOLAME_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libtwolame_plugin.la
codec_PLUGINS += $(LTLIBtwolame)

libdav1d_plugin_la_SOURCES = codec/dav1d.c \
	packetizer/av1_obu.c packetizer/av1_obu.h
libdav1d_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(DAV1D_CFLAGS)
libdav1d_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libdav1d_plugin_la_LIBADD = $(DAV1D_LIBS)
EXTRA_LTLIBRARIES += libdav1d_plugin.la
codec_PLUGINS += $(LTLIBdav1d)


### Hardware encoders ###

libqsv_plugin_la_SOURCES = codec/qsv.c demux/mpeg/timestamps.h
libqsv_plugin_la_CFLAGS = $(AM_CFLAGS) $(MFX_CFLAGS)
libqsv_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libqsv_plugin_la_LIBADD = $(MFX_LIBS)
EXTRA_LTLIBRARIES += libqsv_plugin.la
codec_PLUGINS += $(LTLIBqsv)

### External frameworks ###

libdmo_plugin_la_SOURCES = codec/dmo/dmo.c codec/dmo/dmo.h codec/dmo/buffer.c
libdmo_plugin_la_LIBADD = $(LIBM)
if HAVE_WIN32
libdmo_plugin_la_LIBADD += $(LIBCOM) -luuid
if !HAVE_WINSTORE
codec_PLUGINS += libdmo_plugin.la
endif
endif

libmft_plugin_la_SOURCES = codec/mft.cpp
libmft_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(LIBCOMCXXFLAGS)
if HAVE_WIN32
libmft_plugin_la_LIBADD = $(LIBCOM) -luuid -lmfuuid -lmfplat libvlc_hxxxhelper.la libd3d11_common.la
codec_PLUGINS += libmft_plugin.la
if HAVE_WINSTORE
libmft_plugin_la_LIBADD += -ld3d11
endif
endif

libgstdecode_plugin_la_SOURCES = codec/gstreamer/gstdecode.c \
								 codec/gstreamer/gstvlcpictureplaneallocator.c \
								 codec/gstreamer/gstvlcpictureplaneallocator.h \
								 codec/gstreamer/gstvlcvideopool.c \
								 codec/gstreamer/gstvlcvideopool.h \
								 codec/gstreamer/gstvlcvideosink.c \
								 codec/gstreamer/gstvlcvideosink.h \
								 codec/gstreamer/fourcc.c
libgstdecode_plugin_la_CFLAGS = $(AM_CFLAGS) $(GST_VIDEO_CFLAGS) $(GST_APP_CFLAGS)
libgstdecode_plugin_la_LIBADD = $(GST_VIDEO_LIBS) $(GST_APP_LIBS)
if HAVE_GST_DECODE
codec_PLUGINS += libgstdecode_plugin.la
endif

libvlc_hxxxhelper_la_SOURCES = \
	codec/hxxx_helper.c codec/hxxx_helper.h \
	packetizer/hxxx_nal.h packetizer/hxxx_nal.c \
	packetizer/hxxx_sei.h packetizer/hxxx_sei.c \
	packetizer/h264_slice.c packetizer/h264_slice.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
libvlc_hxxxhelper_la_CPPFLAGS = -Dneedsomethinghere
libvlc_hxxxhelper_la_LDFLAGS = -static
noinst_LTLIBRARIES += libvlc_hxxxhelper.la

libhxxxhelper_testdec_plugin_la_SOURCES = codec/hxxx_helper_testdec.c
libhxxxhelper_testdec_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(codec_RPATH)
libhxxxhelper_testdec_plugin_la_LIBADD = libvlc_hxxxhelper.la
noinst_LTLIBRARIES += libhxxxhelper_testdec_plugin.la

libvlc_vtutils_la_SOURCES = codec/vt_utils.c codec/vt_utils.h
libvlc_vtutils_la_LDFLAGS = -static -no-undefined
EXTRA_LTLIBRARIES += libvlc_vtutils.la
