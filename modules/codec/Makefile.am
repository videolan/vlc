codecdir = $(pluginsdir)/codec
codec_LTLIBRARIES =

AM_CPPFLAGS += -I$(srcdir)/codec

libddummy_plugin_la_SOURCES = codec/ddummy.c
codec_LTLIBRARIES += libddummy_plugin.la

libedummy_plugin_la_SOURCES = codec/edummy.c
if ENABLE_SOUT
codec_LTLIBRARIES += libedummy_plugin.la
endif


### Audio codecs ###
libspdif_plugin_la_SOURCES = codec/spdif.c
codec_LTLIBRARIES += libspdif_plugin.la

liba52_plugin_la_SOURCES = codec/a52.c
liba52_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_a52)
liba52_plugin_la_LIBADD = $(LIBS_a52)
liba52_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += liba52_plugin.la
codec_LTLIBRARIES += $(LTLIBa52)

libdca_plugin_la_SOURCES = codec/dca.c
libdca_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(DCA_CFLAGS)
libdca_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libdca_plugin_la_LIBADD = $(LIBM) $(DCA_LIBS)
EXTRA_LTLIBRARIES += libdca_plugin.la
codec_LTLIBRARIES += $(LTLIBdca)

libadpcm_plugin_la_SOURCES = codec/adpcm.c
codec_LTLIBRARIES += libadpcm_plugin.la

libaes3_plugin_la_SOURCES = codec/aes3.c
codec_LTLIBRARIES += libaes3_plugin.la

libaraw_plugin_la_SOURCES = codec/araw.c
libaraw_plugin_la_LIBADD = $(LIBM)
codec_LTLIBRARIES += libaraw_plugin.la

libfaad_plugin_la_SOURCES = codec/faad.c packetizer/mpeg4audio.h
libfaad_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_faad)
libfaad_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfaad_plugin_la_LIBADD = $(LIBS_faad) $(LIBM)
EXTRA_LTLIBRARIES += libfaad_plugin.la
codec_LTLIBRARIES += $(LTLIBfaad)

libg711_plugin_la_SOURCES = codec/g711.c
codec_LTLIBRARIES += libg711_plugin.la

libfluidsynth_plugin_la_SOURCES = codec/fluidsynth.c
libfluidsynth_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLUIDSYNTH_CFLAGS)
libfluidsynth_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfluidsynth_plugin_la_LIBADD = $(FLUIDSYNTH_LIBS)
EXTRA_LTLIBRARIES += libfluidsynth_plugin.la
codec_LTLIBRARIES += $(LTLIBfluidsynth)
if HAVE_DARWIN
libfluidsynth_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,CoreServices
endif

libaudiotoolboxmidi_plugin_la_SOURCES = codec/audiotoolbox_midi.c
libaudiotoolboxmidi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libaudiotoolboxmidi_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,AudioUnit,-framework,AudioToolbox
EXTRA_LTLIBRARIES += libaudiotoolboxmidi_plugin.la
codec_LTLIBRARIES += $(LTLIBaudiotoolboxmidi)

liblpcm_plugin_la_SOURCES = codec/lpcm.c
codec_LTLIBRARIES += liblpcm_plugin.la

libmad_plugin_la_SOURCES = codec/mad.c
libmad_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(MAD_CFLAGS)
libmad_plugin_la_LIBADD = $(AM_LIBADD) $(MAD_LIBS)
if HAVE_MAD
audio_filter_LTLIBRARIES += libmad_plugin.la
endif

libmpg123_plugin_la_SOURCES = codec/mpg123.c
libmpg123_plugin_la_CFLAGS = $(AM_CFLAGS) $(MPG123_CFLAGS)
libmpg123_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libmpg123_plugin_la_LIBADD = $(MPG123_LIBS)
EXTRA_LTLIBRARIES += libmpg123_plugin.la
codec_LTLIBRARIES += $(LTLIBmpg123)

libuleaddvaudio_plugin_la_SOURCES = codec/uleaddvaudio.c
codec_LTLIBRARIES += libuleaddvaudio_plugin.la

libwma_fixed_plugin_la_SOURCES = \
	codec/wmafixed/asf.h codec/wmafixed/bswap.h \
	codec/wmafixed/fft.c codec/wmafixed/fft.h \
	codec/wmafixed/mdct.c codec/wmafixed/mdct.h \
	codec/wmafixed/wma.c codec/wmafixed/wmadeci.c \
	codec/wmafixed/bitstream.c codec/wmafixed/bitstream.h \
	codec/wmafixed/wmadata.h codec/wmafixed/wmadec.h \
	codec/wmafixed/wmafixed.c codec/wmafixed/wmafixed.h
libwma_fixed_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libwma_fixed_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += libwma_fixed_plugin.la
codec_LTLIBRARIES += $(LTLIBwma_fixed)


### Video codecs ###

libcdg_plugin_la_SOURCES = codec/cdg.c
codec_LTLIBRARIES += libcdg_plugin.la

liblibmpeg2_plugin_la_SOURCES = codec/libmpeg2.c \
	codec/synchro.c codec/synchro.h
liblibmpeg2_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_libmpeg2)
liblibmpeg2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
liblibmpeg2_plugin_la_LIBADD = $(LIBS_libmpeg2)
EXTRA_LTLIBRARIES += liblibmpeg2_plugin.la
codec_LTLIBRARIES += $(LTLIBlibmpeg2)

librawvideo_plugin_la_SOURCES = codec/rawvideo.c
codec_LTLIBRARIES += librawvideo_plugin.la

librtpvideo_plugin_la_SOURCES = codec/rtpvideo.c
if ENABLE_SOUT
codec_LTLIBRARIES += librtpvideo_plugin.la
endif


libschroedinger_plugin_la_SOURCES = codec/schroedinger.c
libschroedinger_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_schroedinger)
libschroedinger_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libschroedinger_plugin_la_LIBADD = $(LIBS_schroedinger)
EXTRA_LTLIBRARIES += libschroedinger_plugin.la
codec_LTLIBRARIES += $(LTLIBschroedinger)


### Image codecs ###

libpng_plugin_la_SOURCES = codec/png.c
libpng_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libpng_plugin_la_LIBADD = -lpng -lz $(LIBM)
EXTRA_LTLIBRARIES += libpng_plugin.la
codec_LTLIBRARIES += $(LTLIBpng)

libjpeg_plugin_la_SOURCES = codec/jpeg.c
libjpeg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libjpeg_plugin_la_LIBADD = -ljpeg
EXTRA_LTLIBRARIES += libjpeg_plugin.la
codec_LTLIBRARIES += $(LTLIBjpeg)

libbpg_plugin_la_SOURCES = codec/bpg.c
libbpg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libbpg_plugin_la_LIBADD = -lbpg
EXTRA_LTLIBRARIES += libbpg_plugin.la
codec_LTLIBRARIES += $(LTLIBbpg)

libsvgdec_plugin_la_SOURCES = codec/svg.c
libsvgdec_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_svgdec)
libsvgdec_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'  $(LDFLAGS_svg)
libsvgdec_plugin_la_LIBADD = $(LIBS_svgdec)
EXTRA_LTLIBRARIES += libsvgdec_plugin.la
codec_LTLIBRARIES += $(LTLIBsvgdec)

libsdl_image_plugin_la_SOURCES = codec/sdl_image.c
libsdl_image_plugin_la_CFLAGS = $(AM_CFLAGS) $(SDL_IMAGE_CFLAGS)
libsdl_image_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libsdl_image_plugin_la_LIBADD = $(SDL_IMAGE_LIBS)
EXTRA_LTLIBRARIES += libsdl_image_plugin.la
codec_LTLIBRARIES += $(LTLIBsdl_image)

libxwd_plugin_la_SOURCES = codec/xwd.c
libxwd_plugin_la_CFLAGS = $(AM_CFLAGS) $(XPROTO_CFLAGS)
libxwd_plugin_la_LIBADD = $(XPROTO_LIBS)
libxwd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
if HAVE_XCB
codec_LTLIBRARIES += libxwd_plugin.la
endif


### SPU ###

liblibass_plugin_la_SOURCES = codec/libass.c
liblibass_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_libass)
liblibass_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBASS_CFLAGS)
liblibass_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
liblibass_plugin_la_LIBADD = $(LIBASS_LIBS) $(LIBS_libass) $(LIBS_freetype) $(FREETYPE_LIBS)
if HAVE_WINSTORE
liblibass_plugin_la_LIBADD += -ldwrite
endif
EXTRA_LTLIBRARIES += liblibass_plugin.la
codec_LTLIBRARIES += $(LTLIBlibass)

libcc_plugin_la_SOURCES = codec/cc.c codec/cc.h codec/substext.h \
                          codec/cea708.h codec/cea708.c
codec_LTLIBRARIES += libcc_plugin.la

libcvdsub_plugin_la_SOURCES = codec/cvdsub.c
codec_LTLIBRARIES += libcvdsub_plugin.la

libdvbsub_plugin_la_SOURCES = codec/dvbsub.c
codec_LTLIBRARIES += libdvbsub_plugin.la

libaribsub_plugin_la_SOURCES = codec/arib/aribsub.c codec/arib/substext.h
libaribsub_plugin_la_CFLAGS = $(AM_CFLAGS) $(ARIBB24_CFLAGS)
libaribsub_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libaribsub_plugin_la_LIBADD = $(ARIBB24_LIBS)
EXTRA_LTLIBRARIES += libaribsub_plugin.la
codec_LTLIBRARIES += $(LTLIBaribsub)

libscte18_plugin_la_SOURCES = codec/scte18.c codec/scte18.h \
                              codec/atsc_a65.c codec/atsc_a65.h
codec_LTLIBRARIES += libscte18_plugin.la

libscte27_plugin_la_SOURCES = codec/scte27.c
codec_LTLIBRARIES += libscte27_plugin.la

libspudec_plugin_la_SOURCES = codec/spudec/spudec.c codec/spudec/parse.c codec/spudec/spudec.h
libspudec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
codec_LTLIBRARIES += libspudec_plugin.la

libstl_plugin_la_SOURCES = codec/stl.c
codec_LTLIBRARIES += libstl_plugin.la

libsubsdec_plugin_la_SOURCES = codec/subsdec.c codec/substext.h
codec_LTLIBRARIES += libsubsdec_plugin.la

libsubsusf_plugin_la_SOURCES = codec/subsusf.c
codec_LTLIBRARIES += libsubsusf_plugin.la

libttml_plugin_la_SOURCES = codec/ttml/substtml.c \
                            demux/ttml.c \
                            codec/ttml/ttml.h codec/ttml/ttml.c
codec_LTLIBRARIES += libttml_plugin.la

libwebvtt_plugin_la_SOURCES = codec/webvtt/subsvtt.c \
                              codec/webvtt/webvtt.c \
                              codec/webvtt/webvtt.h \
                              demux/webvtt.c \
                              demux/mp4/minibox.h
codec_LTLIBRARIES += libwebvtt_plugin.la
if ENABLE_CSS
# dependency: we need grammar build first
BUILT_SOURCES += codec/webvtt/CSSGrammar.h
libwebvtt_plugin_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/codec/webvtt/ -I$(builddir)/codec/webvtt/
libwebvtt_plugin_la_SOURCES += codec/webvtt/CSSGrammar.y \
                               codec/webvtt/CSSLexer.l \
                               codec/webvtt/css_parser.c \
                               codec/webvtt/css_parser.h \
                               codec/webvtt/css_style.c \
                               codec/webvtt/css_style.h \
                               codec/webvtt/css_bridge.h
endif

libsvcdsub_plugin_la_SOURCES = codec/svcdsub.c
codec_LTLIBRARIES += libsvcdsub_plugin.la

libt140_plugin_la_SOURCES = codec/t140.c
if ENABLE_SOUT
codec_LTLIBRARIES += libt140_plugin.la
endif

libtelx_plugin_la_SOURCES = codec/telx.c
libtelx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += libtelx_plugin.la
codec_LTLIBRARIES += $(LTLIBtelx)

libtextst_plugin_la_SOURCES = codec/textst.c
libtextst_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
codec_LTLIBRARIES += libtextst_plugin.la

libzvbi_plugin_la_SOURCES = codec/zvbi.c
libzvbi_plugin_la_CFLAGS = $(AM_CFLAGS) $(ZVBI_CFLAGS) $(CFLAGS_zvbi)
libzvbi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libzvbi_plugin_la_LIBADD = $(ZVBI_LIBS) $(SOCKET_LIBS)
if HAVE_WIN32
libzvbi_plugin_la_LIBADD += -lpthreadGC2
endif
EXTRA_LTLIBRARIES += libzvbi_plugin.la
codec_LTLIBRARIES += $(LTLIBzvbi)

libsubstx3g_plugin_la_SOURCES = codec/substx3g.c codec/substext.h
codec_LTLIBRARIES += libsubstx3g_plugin.la

### Xiph ###

libflac_plugin_la_SOURCES = codec/flac.c
libflac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLAC_CFLAGS)
libflac_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libflac_plugin_la_LIBADD = $(FLAC_LIBS)
EXTRA_LTLIBRARIES += libflac_plugin.la
codec_LTLIBRARIES += $(LTLIBflac)

libkate_plugin_la_SOURCES = codec/kate.c
libkate_plugin_la_CFLAGS = $(AM_CFLAGS) $(TIGER_CFLAGS) $(KATE_CFLAGS)
libkate_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libkate_plugin_la_LIBADD = $(TIGER_LIBS) $(KATE_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libkate_plugin.la
codec_LTLIBRARIES += $(LTLIBkate)

libopus_plugin_la_SOURCES = codec/opus.c codec/opus_header.c codec/opus_header.h
libopus_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPUS_CFLAGS)
libopus_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libopus_plugin_la_LIBADD = $(OPUS_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libopus_plugin.la
codec_LTLIBRARIES += $(LTLIBopus)

libspeex_plugin_la_SOURCES = codec/speex.c
libspeex_plugin_la_CFLAGS = $(AM_CFLAGS) $(SPEEX_CFLAGS)
libspeex_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libspeex_plugin_la_LIBADD = $(SPEEX_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libspeex_plugin.la
codec_LTLIBRARIES += $(LTLIBspeex)

libtheora_plugin_la_SOURCES = codec/theora.c
libtheora_plugin_la_CFLAGS = $(AM_CFLAGS) $(THEORA_CFLAGS)
libtheora_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtheora_plugin_la_LIBADD = $(THEORA_LIBS)
EXTRA_LTLIBRARIES += libtheora_plugin.la
codec_LTLIBRARIES += $(LTLIBtheora)

libdaala_plugin_la_SOURCES = codec/daala.c
libdaala_plugin_la_CFLAGS = $(AM_CFLAGS) $(DAALA_CFLAGS)
libdaala_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libdaala_plugin_la_LIBADD = $(DAALA_LIBS)
EXTRA_LTLIBRARIES += libdaala_plugin.la
codec_LTLIBRARIES += $(LTLIBdaala)

libtremor_plugin_la_SOURCES = codec/vorbis.c
libtremor_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_tremor
libtremor_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtremor_plugin_la_LIBADD = -lvorbisdec -logg
EXTRA_LTLIBRARIES += libtremor_plugin.la
codec_LTLIBRARIES += $(LTLIBtremor)

libvorbis_plugin_la_SOURCES = codec/vorbis.c
libvorbis_plugin_la_CFLAGS = $(AM_CFLAGS) $(VORBIS_CFLAGS)
libvorbis_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libvorbis_plugin_la_LIBADD = $(VORBIS_LIBS)
EXTRA_LTLIBRARIES += libvorbis_plugin.la
codec_LTLIBRARIES += $(LTLIBvorbis)

liboggspots_plugin_la_SOURCES = codec/oggspots.c
liboggspots_plugin_la_CFLAGS = $(AM_CFLAGS) $(OGGSPOTS_CFLAGS)
liboggspots_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
liboggspots_plugin_la_LIBADD = $(OGGSPOTS_LIBS)
EXTRA_LTLIBRARIES += liboggspots_plugin.la
codec_LTLIBRARIES += $(LTLIBoggspots)

libvideotoolbox_plugin_la_SOURCES = \
	codec/vt_utils.c codec/vt_utils.h \
	codec/videotoolbox.m codec/hxxx_helper.c codec/hxxx_helper.h \
	packetizer/hxxx_nal.h packetizer/hxxx_nal.c \
	packetizer/hxxx_sei.h packetizer/hxxx_sei.c \
	packetizer/h264_slice.c packetizer/h264_slice.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
if HAVE_OSX
libvideotoolbox_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -mmacosx-version-min=10.8
endif
if HAVE_IOS
libvideotoolbox_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -miphoneos-version-min=8.0
endif
if HAVE_TVOS
libvideotoolbox_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -mtvos-version-min=10.2
endif
libvideotoolbox_plugin_la_LIBADD = libchroma_copy.la
libvideotoolbox_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)' -Wl,-framework,Foundation -Wl,-framework,VideoToolbox -Wl,-framework,CoreMedia -Wl,-framework,CoreVideo
EXTRA_LTLIBRARIES += libvideotoolbox_plugin.la
codec_LTLIBRARIES += $(LTLIBvideotoolbox)

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
codec_LTLIBRARIES += libavcodec_plugin.la
endif

### avcodec hardware acceleration ###

libvaapi_drm_plugin_la_SOURCES = \
	codec/avcodec/vaapi.c hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libvaapi_drm_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DVLC_VA_BACKEND_DRM
libvaapi_drm_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(LIBVA_DRM_CFLAGS) $(AVCODEC_CFLAGS)
libvaapi_drm_plugin_la_LIBADD = $(LIBVA_DRM_LIBS)
libvaapi_plugin_la_SOURCES = \
	codec/avcodec/vaapi.c hw/vaapi/vlc_vaapi.c hw/vaapi/vlc_vaapi.h
libvaapi_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvaapi_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVCODEC_CFLAGS)
libvaapi_plugin_la_LIBADD = $(LIBVA_LIBS)
if HAVE_AVCODEC_VAAPI
if HAVE_VAAPI_DRM
codec_LTLIBRARIES += libvaapi_drm_plugin.la
endif
codec_LTLIBRARIES += libvaapi_plugin.la
endif

libd3d9_common_la_SOURCES = video_chroma/d3d9_fmt.c video_chroma/d3d9_fmt.h \
	video_chroma/dxgi_fmt.c video_chroma/dxgi_fmt.h
libd3d9_common_la_LDFLAGS = -static
if HAVE_WIN32_DESKTOP
noinst_LTLIBRARIES += libd3d9_common.la
endif

libdxva2_plugin_la_SOURCES = \
	codec/avcodec/dxva2.c codec/avcodec/directx_va.c codec/avcodec/directx_va.h \
	codec/avcodec/va_surface.c codec/avcodec/va_surface.h codec/avcodec/va_surface_internal.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
libdxva2_plugin_la_LIBADD = libd3d9_common.la $(LIBCOM) -lshlwapi -luuid
if HAVE_AVCODEC_DXVA2
codec_LTLIBRARIES += libdxva2_plugin.la
endif

libd3d11_common_la_SOURCES = video_chroma/d3d11_fmt.c video_chroma/d3d11_fmt.h \
	video_chroma/dxgi_fmt.c video_chroma/dxgi_fmt.h
libd3d11_common_la_LDFLAGS = -static
if HAVE_WIN32
noinst_LTLIBRARIES += libd3d11_common.la
endif

libd3d11va_plugin_la_SOURCES = \
	codec/avcodec/d3d11va.c codec/avcodec/directx_va.c codec/avcodec/directx_va.h \
        codec/avcodec/va_surface.c codec/avcodec/va_surface.h codec/avcodec/va_surface_internal.h \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
libd3d11va_plugin_la_LIBADD = libd3d11_common.la $(LIBCOM) -luuid
if HAVE_WINSTORE
libd3d11va_plugin_la_LIBADD += -ld3d11
endif
if HAVE_AVCODEC_D3D11VA
codec_LTLIBRARIES += libd3d11va_plugin.la
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
if HAVE_ANDROID
libomxil_plugin_la_SOURCES += video_output/android/utils.c video_output/android/utils.h \
	video_output/android/display.h
endif
libomxil_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/codec/omxil $(CFLAGS_omxil)
libomxil_plugin_la_LIBADD = libchroma_copy.la $(LIBDL)
libomxil_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'

libomxil_vout_plugin_la_SOURCES = codec/omxil/vout.c codec/omxil/omxil_core.c codec/omxil/utils.c codec/omxil/qcom.c
libomxil_vout_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/codec/omxil $(CFLAGS_omxil_vout)
libomxil_vout_plugin_la_LIBADD = libchroma_copy.la $(LIBDL)
libomxil_vout_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'

libiomx_plugin_la_SOURCES = $(libomxil_plugin_la_SOURCES)
libiomx_plugin_la_CPPFLAGS = $(libomxil_plugin_la_CPPFLAGS) -DUSE_IOMX
libiomx_plugin_la_LIBADD = $(libomxil_plugin_la_LIBADD)

libmediacodec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/codec/omxil
libmediacodec_plugin_la_SOURCES = codec/omxil/mediacodec.c codec/omxil/mediacodec.h \
	codec/omxil/mediacodec_jni.c codec/omxil/mediacodec_ndk.c codec/omxil/utils.c \
	video_output/android/utils.c video_output/android/utils.h \
	video_output/android/display.h \
	codec/hxxx_helper.c codec/hxxx_helper.h \
	packetizer/hxxx_nal.h packetizer/hxxx_nal.c \
	packetizer/h264_nal.c packetizer/h264_nal.h \
	packetizer/hevc_nal.c packetizer/hevc_nal.h
libmediacodec_plugin_la_LIBADD = libchroma_copy.la 
codec_LTLIBRARIES += $(LTLIBomxil) $(LTLIBomxil_vout)
EXTRA_LTLIBRARIES += libomxil_plugin.la libomxil_vout_plugin.la
if HAVE_ANDROID
codec_LTLIBRARIES += libiomx_plugin.la libmediacodec_plugin.la
endif


### X26x encoders ###

libx265_plugin_la_SOURCES = codec/x265.c
libx265_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libx265_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x265)
libx265_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x265) -rpath '$(codecdir)'
libx265_plugin_la_LIBADD = $(LIBS_x265)
EXTRA_LTLIBRARIES += libx265_plugin.la
codec_LTLIBRARIES += $(LTLIBx265)

libx262_plugin_la_SOURCES = codec/x264.c
libx262_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_x262
libx262_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x262)
libx262_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x262) -rpath '$(codecdir)'
libx262_plugin_la_LIBADD = $(LIBS_x262) $(LIBM)
EXTRA_LTLIBRARIES += libx262_plugin.la
codec_LTLIBRARIES += $(LTLIBx262)

libx264_plugin_la_SOURCES = codec/x264.c
libx264_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_x264) -DMODULE_NAME_IS_x264
libx264_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x264)
libx264_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x264) -rpath '$(codecdir)'
libx264_plugin_la_LIBADD = $(LIBS_x264) $(LIBM)
EXTRA_LTLIBRARIES += libx264_plugin.la
codec_LTLIBRARIES += $(LTLIBx264)

libx26410b_plugin_la_SOURCES = codec/x264.c
libx26410b_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_x26410b
libx26410b_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x26410b)
libx26410b_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x26410b) -rpath '$(codecdir)'
libx26410b_plugin_la_LIBADD = $(LIBS_x26410b) $(LIBM)
EXTRA_LTLIBRARIES += libx26410b_plugin.la
codec_LTLIBRARIES += $(LTLIBx26410b)


### Misc encoders ###

libfdkaac_plugin_la_SOURCES = codec/fdkaac.c
libfdkaac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FDKAAC_CFLAGS)
libfdkaac_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfdkaac_plugin_la_LIBADD = $(FDKAAC_LIBS)
EXTRA_LTLIBRARIES += libfdkaac_plugin.la
codec_LTLIBRARIES += $(LTLIBfdkaac)

libshine_plugin_la_SOURCES = codec/shine.c
libshine_plugin_la_CFLAGS = $(AM_CFLAGS) $(SHINE_CFLAGS)
libshine_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)' $(SHINE_LIBS)
EXTRA_LTLIBRARIES += libshine_plugin.la
codec_LTLIBRARIES += $(LTLIBshine)

libvpx_plugin_la_SOURCES = codec/vpx.c
libvpx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libvpx_plugin_la_CFLAGS = $(AM_CFLAGS) $(VPX_CFLAGS) $(CPPFLAGS_vpx)
libvpx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libvpx_plugin_la_LIBADD = $(VPX_LIBS)
EXTRA_LTLIBRARIES += libvpx_plugin.la
codec_LTLIBRARIES += $(LTLIBvpx)

libaom_plugin_la_SOURCES = codec/aom.c
libaom_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libaom_plugin_la_CFLAGS = $(AM_CFLAGS) $(AOM_CFLAGS) $(CPPFLAGS_aom)
libaom_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libaom_plugin_la_LIBADD = $(AOM_LIBS) $(LIBPTHREAD)
EXTRA_LTLIBRARIES += libaom_plugin.la
codec_LTLIBRARIES += $(LTLIBaom)

libtwolame_plugin_la_SOURCES = codec/twolame.c
libtwolame_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DLIBTWOLAME_STATIC
libtwolame_plugin_la_CFLAGS = $(AM_CFLAGS) $(TWOLAME_CFLAGS)
libtwolame_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtwolame_plugin_la_LIBADD = $(TWOLAME_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libtwolame_plugin.la
codec_LTLIBRARIES += $(LTLIBtwolame)


### Hardware encoders ###

libcrystalhd_plugin_la_SOURCES = codec/crystalhd.c packetizer/h264_nal.c packetizer/h264_nal.h
libcrystalhd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libcrystalhd_plugin_la_LIBADD = $(LIBS_crystalhd)
EXTRA_LTLIBRARIES += libcrystalhd_plugin.la
codec_LTLIBRARIES += $(LTLIBcrystalhd)

libqsv_plugin_la_SOURCES = codec/qsv.c
libqsv_plugin_la_CFLAGS = $(AM_CFLAGS) $(MFX_CFLAGS)
libqsv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libqsv_plugin_la_LIBADD = $(MFX_LIBS)
EXTRA_LTLIBRARIES += libqsv_plugin.la
codec_LTLIBRARIES += $(LTLIBqsv)


### External frameworks ###

libdmo_plugin_la_SOURCES = codec/dmo/dmo.c codec/dmo/dmo.h codec/dmo/buffer.c
libqsv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libdmo_plugin_la_LIBADD = $(LIBM)
if HAVE_WIN32
libdmo_plugin_la_LIBADD += $(LIBCOM) -luuid
codec_LTLIBRARIES += libdmo_plugin.la
endif

libmft_plugin_la_SOURCES = codec/mft.c packetizer/h264_nal.c packetizer/h264_nal.h
if HAVE_WIN32
libmft_plugin_la_LIBADD = $(LIBCOM) -luuid -lmfuuid -lmfplat
codec_LTLIBRARIES += libmft_plugin.la
endif

libgstdecode_plugin_la_SOURCES = codec/gstreamer/gstdecode.c \
								 codec/gstreamer/gstvlcpictureplaneallocator.c \
								 codec/gstreamer/gstvlcpictureplaneallocator.h \
								 codec/gstreamer/gstvlcvideopool.c \
								 codec/gstreamer/gstvlcvideopool.h \
								 codec/gstreamer/gstvlcvideosink.c \
								 codec/gstreamer/gstvlcvideosink.h
libgstdecode_plugin_la_CFLAGS = $(AM_CFLAGS) $(GST_VIDEO_CFLAGS) $(GST_APP_CFLAGS)
libgstdecode_plugin_la_LIBADD = $(GST_VIDEO_LIBS) $(GST_APP_LIBS)
if HAVE_GST_DECODE
codec_LTLIBRARIES += libgstdecode_plugin.la
endif
