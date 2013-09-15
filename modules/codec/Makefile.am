include ../common.am
codecdir = $(pluginsdir)/codec
codec_LTLIBRARIES =
EXTRA_LTLIBRARIES =
noinst_HEADERS =
noinst_LTLIBRARIES =

libddummy_plugin_la_SOURCES = ddummy.c
codec_LTLIBRARIES += libddummy_plugin.la

libedummy_plugin_la_SOURCES = edummy.c
if ENABLE_SOUT
codec_LTLIBRARIES += libedummy_plugin.la
endif


### Audio codecs ###

liba52_plugin_la_SOURCES = a52.c a52.h
codec_LTLIBRARIES += liba52_plugin.la

libadpcm_plugin_la_SOURCES = adpcm.c
codec_LTLIBRARIES += libadpcm_plugin.la

libaes3_plugin_la_SOURCES = aes3.c
codec_LTLIBRARIES += libaes3_plugin.la

libaraw_plugin_la_SOURCES = araw.c
codec_LTLIBRARIES += libaraw_plugin.la

libdts_plugin_la_SOURCES = dts.c
libdts_plugin_la_LIBADD = libvlc_dts_header.la
codec_LTLIBRARIES += libdts_plugin.la

libvlc_dts_header_la_SOURCES = dts_header.c dts_header.h
libvlc_dts_header_la_LDFLAGS = -static
noinst_LTLIBRARIES += libvlc_dts_header.la

libfaad_plugin_la_SOURCES = faad.c
libfaad_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_faad)
libfaad_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfaad_plugin_la_LIBADD = $(LIBS_faad) $(LIBM)
EXTRA_LTLIBRARIES += libfaad_plugin.la
codec_LTLIBRARIES += $(LTLIBfaad)

libg711_plugin_la_SOURCES = g711.c
codec_LTLIBRARIES += libg711_plugin.la

libfluidsynth_plugin_la_SOURCES = fluidsynth.c
libfluidsynth_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLUIDSYNTH_CFLAGS)
libfluidsynth_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfluidsynth_plugin_la_LIBADD = $(FLUIDSYNTH_LIBS)
EXTRA_LTLIBRARIES += libfluidsynth_plugin.la
codec_LTLIBRARIES += $(LTLIBfluidsynth)

liblpcm_plugin_la_SOURCES = lpcm.c
codec_LTLIBRARIES += liblpcm_plugin.la

libmpeg_audio_plugin_la_SOURCES = mpeg_audio.c
codec_LTLIBRARIES += libmpeg_audio_plugin.la

libuleaddvaudio_plugin_la_SOURCES = uleaddvaudio.c
codec_LTLIBRARIES += libuleaddvaudio_plugin.la

libwma_fixed_plugin_la_SOURCES = \
	wmafixed/asf.h wmafixed/bswap.h \
	wmafixed/fft.c wmafixed/fft.h \
	wmafixed/mdct.c wmafixed/mdct.h \
	wmafixed/wma.c wmafixed/wmadeci.c \
	wmafixed/bitstream.c wmafixed/bitstream.h \
	wmafixed/wmadata.h wmafixed/wmadec.h \
	wmafixed/wmafixed.c wmafixed/wmafixed.h
libwma_fixed_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libwma_fixed_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += libwma_fixed_plugin.la
codec_LTLIBRARIES += $(LTLIBwma_fixed)


### Video codecs ###

libcdg_plugin_la_SOURCES = cdg.c
codec_LTLIBRARIES += libcdg_plugin.la

liblibmpeg2_plugin_la_SOURCES = libmpeg2.c
liblibmpeg2_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_libmpeg2)
liblibmpeg2_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
liblibmpeg2_plugin_la_LIBADD = $(LIBS_libmpeg2)
EXTRA_LTLIBRARIES += liblibmpeg2_plugin.la
codec_LTLIBRARIES += $(LTLIBlibmpeg2)

librawvideo_plugin_la_SOURCES = rawvideo.c
codec_LTLIBRARIES += librawvideo_plugin.la

libschroedinger_plugin_la_SOURCES = schroedinger.c
libschroedinger_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_schroedinger)
libschroedinger_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libschroedinger_plugin_la_LIBADD = $(LIBS_schroedinger)
EXTRA_LTLIBRARIES += libschroedinger_plugin.la
codec_LTLIBRARIES += $(LTLIBschroedinger)


### Image codecs ###

libpng_plugin_la_SOURCES = png.c
libpng_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libpng_plugin_la_LIBADD = -lpng -lz $(LIBM)
EXTRA_LTLIBRARIES += libpng_plugin.la
codec_LTLIBRARIES += $(LTLIBpng)

libsdl_image_plugin_la_SOURCES = sdl_image.c
libsdl_image_plugin_la_CFLAGS = $(AM_CFLAGS) $(SDL_IMAGE_CFLAGS)
libsdl_image_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libsdl_image_plugin_la_LIBADD = $(SDL_IMAGE_LIBS)
EXTRA_LTLIBRARIES += libsdl_image_plugin.la
codec_LTLIBRARIES += $(LTLIBsdl_image)

libxwd_plugin_la_SOURCES = xwd.c
libxwd_plugin_la_CFLAGS = $(AM_CFLAGS) $(XPROTO_CFLAGS)
libxwd_plugin_la_LIBADD = $(XPROTO_LIBS)
libxwd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
codec_LTLIBRARIES += $(LTLIBxwd)
EXTRA_LTLIBRARIES += libxwd_plugin.la


### SPU ###

liblibass_plugin_la_SOURCES = libass.c
liblibass_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_libass)
liblibass_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBASS_CFLAGS)
liblibass_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
liblibass_plugin_la_LIBADD = $(LIBASS_LIBS) $(LIBS_libass)
EXTRA_LTLIBRARIES += liblibass_plugin.la
codec_LTLIBRARIES += $(LTLIBlibass)

libcc_plugin_la_SOURCES = cc.c cc.h substext.h
codec_LTLIBRARIES += libcc_plugin.la

libcvdsub_plugin_la_SOURCES = cvdsub.c
codec_LTLIBRARIES += libcvdsub_plugin.la

libdvbsub_plugin_la_SOURCES = dvbsub.c
codec_LTLIBRARIES += libdvbsub_plugin.la

libscte27_plugin_la_SOURCES = scte27.c
codec_LTLIBRARIES += libscte27_plugin.la

libspudec_plugin_la_SOURCES = spudec/spudec.c spudec/parse.c spudec/spudec.h
libspudec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
codec_LTLIBRARIES += libspudec_plugin.la

libstl_plugin_la_SOURCES = stl.c
codec_LTLIBRARIES += libstl_plugin.la

libsubsdec_plugin_la_SOURCES = subsdec.c substext.h
codec_LTLIBRARIES += libsubsdec_plugin.la

libsubsusf_plugin_la_SOURCES = subsusf.c
codec_LTLIBRARIES += libsubsusf_plugin.la

libsvcdsub_plugin_la_SOURCES = svcdsub.c
codec_LTLIBRARIES += libsvcdsub_plugin.la

libt140_plugin_la_SOURCES = t140.c
if ENABLE_SOUT
codec_LTLIBRARIES += libt140_plugin.la
endif

libtelx_plugin_la_SOURCES = telx.c
libtelx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += libtelx_plugin.la
codec_LTLIBRARIES += $(LTLIBtelx)

libzvbi_plugin_la_SOURCES = zvbi.c
libzvbi_plugin_la_CFLAGS = $(AM_CFLAGS) $(ZVBI_CFLAGS) $(CFLAGS_zvbi)
libzvbi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libzvbi_plugin_la_LIBADD = $(ZVBI_LIBS) $(SOCKET_LIBS)
if HAVE_WIN32
libzvbi_plugin_la_LIBADD += -lpthreadGC2
endif
EXTRA_LTLIBRARIES += libzvbi_plugin.la
codec_LTLIBRARIES += $(LTLIBzvbi)


### Xiph ###

libflac_plugin_la_SOURCES = flac.c
libflac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FLAC_CFLAGS)
libflac_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libflac_plugin_la_LIBADD = $(FLAC_LIBS)
EXTRA_LTLIBRARIES += libflac_plugin.la
codec_LTLIBRARIES += $(LTLIBflac)

libkate_plugin_la_SOURCES = kate.c
libkate_plugin_la_CFLAGS = $(AM_CFLAGS) $(TIGER_CFLAGS) $(KATE_CFLAGS)
libkate_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libkate_plugin_la_LIBADD = $(TIGER_LIBS) $(KATE_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libkate_plugin.la
codec_LTLIBRARIES += $(LTLIBkate)

libopus_plugin_la_SOURCES = opus.c opus_header.c opus_header.h
libopus_plugin_la_CFLAGS = $(AM_CFLAGS) $(OPUS_CFLAGS)
libopus_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libopus_plugin_la_LIBADD = $(OPUS_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libopus_plugin.la
codec_LTLIBRARIES += $(LTLIBopus)

libspeex_plugin_la_SOURCES = speex.c
libspeex_plugin_la_CFLAGS = $(AM_CFLAGS) $(SPEEX_CFLAGS)
libspeex_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libspeex_plugin_la_LIBADD = $(SPEEX_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libspeex_plugin.la
codec_LTLIBRARIES += $(LTLIBspeex)

libtheora_plugin_la_SOURCES = theora.c
libtheora_plugin_la_CFLAGS = $(AM_CFLAGS) $(THEORA_CFLAGS)
libtheora_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtheora_plugin_la_LIBADD = $(THEORA_LIBS)
EXTRA_LTLIBRARIES += libtheora_plugin.la
codec_LTLIBRARIES += $(LTLIBtheora)

libtremor_plugin_la_SOURCES = vorbis.c
libtremor_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_tremor
libtremor_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtremor_plugin_la_LIBADD = -lvorbisdec -logg
EXTRA_LTLIBRARIES += libtremor_plugin.la
codec_LTLIBRARIES += $(LTLIBtremor)

libvorbis_plugin_la_SOURCES = vorbis.c
libvorbis_plugin_la_CFLAGS = $(AM_CFLAGS) $(VORBIS_CFLAGS)
libvorbis_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libvorbis_plugin_la_LIBADD = $(VORBIS_LIBS)
EXTRA_LTLIBRARIES += libvorbis_plugin.la
codec_LTLIBRARIES += $(LTLIBvorbis)


### FFmpeg/libav ###

libavcodec_plugin_la_SOURCES = \
	avcodec/avcommon_compat.h \
	avcodec/avcommon.h \
	avcodec/video.c \
	avcodec/subtitle.c \
	avcodec/audio.c \
	avcodec/cpu.c \
	avcodec/fourcc.c \
	avcodec/chroma.c avcodec/chroma.h \
	avcodec/va.c avcodec/va.h \
	avcodec/avcodec.c avcodec/avcodec.h
if ENABLE_SOUT
libavcodec_plugin_la_SOURCES += avcodec/encoder.c
endif
libavcodec_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVCODEC_CFLAGS)
libavcodec_plugin_la_LIBADD = $(AVCODEC_LIBS) $(LIBM)
libavcodec_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(SYMBOLIC_LDFLAGS)
if HAVE_WIN32
libavcodec_plugin_la_DEPENDENCIES = libavcodec_plugin.rc.o
endif

if MERGE_FFMPEG
libavcodec_plugin_la_SOURCES += \
	../demux/avformat/demux.c \
	../access/avio.c
libavcodec_plugin_la_CFLAGS += $(AVFORMAT_CFLAGS)
libavcodec_plugin_la_LIBADD += $(AVFORMAT_LIBS)
if ENABLE_SOUT
libavcodec_plugin_la_SOURCES += \
	../demux/avformat/mux.c
endif
libavcodec_plugin_la_CFLAGS += -DMERGE_FFMPEG
endif
if HAVE_AVCODEC
codec_LTLIBRARIES += libavcodec_plugin.la
endif

### avcodec hardware acceleration ###

libvaapi_plugin_la_SOURCES = \
	avcodec/copy.c avcodec/copy.h \
	avcodec/vaapi.c
libvaapi_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(LIBVA_CFLAGS) $(X_CFLAGS) $(AVCODEC_CFLAGS)
libvaapi_plugin_la_LIBADD = $(LIBVA_LIBS) $(X_LIBS) $(X_PRE_LIBS) -lX11
if HAVE_AVCODEC_VAAPI
codec_LTLIBRARIES += libvaapi_plugin.la
endif

libdxva2_plugin_la_SOURCES = \
	avcodec/copy.c avcodec/copy.h \
	avcodec/dxva2.c
libdxva2_plugin_la_LIBADD = -lole32 -lshlwapi -luuid
if HAVE_AVCODEC_DXVA2
codec_LTLIBRARIES += libdxva2_plugin.la
endif
libdxva2_plugin_la_DEPENDENCIES = libdxva2_plugin.rc.o

libvda_plugin_la_SOURCES = \
	avcodec/copy.c avcodec/copy.h \
	avcodec/vda.c
libvda_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVCODEC_CFLAGS)
libvda_plugin_la_LDFLAGS = -Wl,-framework,CoreFoundation,-framework,VideoDecodeAcceleration,-framework,QuartzCore
if HAVE_AVCODEC_VDA
codec_LTLIBRARIES += libvda_plugin.la
endif

libhwdummy_plugin_la_SOURCES = avcodec/hwdummy.c
libhwdummy_plugin_la_CFLAGS = $(AM_CFLAGS) $(VDPAU_CFLAGS) $(AVCODEC_CFLAGS)
libhwdummy_plugin_la_LIBADD = $(AVCODEC_LIBS)
# FIXME: hwdummy only needs libavutil, and only if avcodec < 55.13.0
if HAVE_AVCODEC_VDPAU
codec_LTLIBRARIES += libhwdummy_plugin.la
endif
if HAVE_WIN32
libhwdummy_plugin_la_DEPENDENCIES = libhwdummy_plugin.rc.o
endif


### OpenMAX ###

noinst_HEADERS += \
	omxil/OMX_Broadcom.h \
	omxil/OMX_Component.h \
	omxil/OMX_Core.h \
	omxil/OMX_Image.h \
	omxil/OMX_IVCommon.h \
	omxil/OMX_Types.h \
	omxil/OMX_Audio.h \
	omxil/OMX_Index.h \
	omxil/OMX_Other.h \
	omxil/OMX_Video.h 

libomxil_plugin_la_SOURCES = \
	omxil/utils.c omxil/omxil_utils.h \
	h264_nal.h \
	omxil/qcom.c omxil/qcom.h \
	omxil/omxil.c omxil/omxil.h omxil/omxil_core.c omxil/omxil_core.h
libomxil_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/omxil $(CFLAGS_omxil)
libomxil_plugin_la_LIBADD = $(LIBDL)
libomxil_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'

libomxil_vout_plugin_la_SOURCES = omxil/vout.c omxil/omxil_core.c omxil/utils.c omxil/qcom.c
libomxil_vout_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/omxil $(CFLAGS_omxil_vout)
libomxil_vout_plugin_la_LIBADD = $(LIBDL)
libomxil_vout_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'

libiomx_plugin_la_SOURCES = $(libomxil_plugin_la_SOURCES)
libiomx_plugin_la_CPPFLAGS = $(libomxil_plugin_la_CPPFLAGS) -DUSE_IOMX
libiomx_plugin_la_LIBADD = $(libomxil_plugin_la_LIBADD)

libmediacodec_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/omxil
libmediacodec_plugin_la_SOURCES = omxil/android_mediacodec.c omxil/utils.c

codec_LTLIBRARIES += $(LTLIBomxil) $(LTLIBomxil_vout)
EXTRA_LTLIBRARIES += libomxil_plugin.la libomxil_vout_plugin.la
if HAVE_ANDROID
codec_LTLIBRARIES += libiomx_plugin.la libmediacodec_plugin.la
endif


### X26x encoders ###

libx262_plugin_la_SOURCES = x264.c
libx262_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_x262
libx262_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x262)
libx262_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x262)
libx262_plugin_la_LIBADD = $(LIBS_x262)
EXTRA_LTLIBRARIES += libx262_plugin.la
codec_LTLIBRARIES += $(LTLIBx262)

libx264_plugin_la_SOURCES = x264.c
libx264_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_x264) -DMODULE_NAME_IS_x264
libx264_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x264)
libx264_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x264)
libx264_plugin_la_LIBADD = $(LIBS_x264)
EXTRA_LTLIBRARIES += libx264_plugin.la
codec_LTLIBRARIES += $(LTLIBx264)

libx26410b_plugin_la_SOURCES = x264.c
libx26410b_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -DMODULE_NAME_IS_x26410b
libx26410b_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_x26410b)
libx26410b_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_x26410b)
libx26410b_plugin_la_LIBADD = $(LIBS_x26410b)
EXTRA_LTLIBRARIES += libx26410b_plugin.la
codec_LTLIBRARIES += $(LTLIBx26410b)


### Misc encoders ###

libdirac_plugin_la_SOURCES = dirac.c
libdirac_plugin_la_CFLAGS = $(AM_CFLAGS) $(DIRAC_CFLAGS)
libdirac_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libdirac_plugin_la_LIBADD = $(DIRAC_LIBS)
EXTRA_LTLIBRARIES += libdirac_plugin.la
codec_LTLIBRARIES += $(LTLIBdirac)

libfdkaac_plugin_la_SOURCES = fdkaac.c
libfdkaac_plugin_la_CFLAGS = $(AM_CFLAGS) $(FDKAAC_CFLAGS)
libfdkaac_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libfdkaac_plugin_la_LIBADD = $(FDKAAC_LIBS)
EXTRA_LTLIBRARIES += libfdkaac_plugin.la
codec_LTLIBRARIES += $(LTLIBfdkaac)

libshine_plugin_la_SOURCES = shine/shine_mod.c shine/enc_base.h
EXTRA_libshine_plugin_la_SOURCES = shine/shine.c
libshine_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
EXTRA_LTLIBRARIES += libshine_plugin.la
codec_LTLIBRARIES += $(LTLIBshine)

libtwolame_plugin_la_SOURCES = twolame.c
libtwolame_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libtwolame_plugin_la_CFLAGS = $(AM_CFLAGS) $(TWOLAME_CFLAGS)
libtwolame_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libtwolame_plugin_la_LIBADD = $(TWOLAME_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libtwolame_plugin.la
codec_LTLIBRARIES += $(LTLIBtwolame)


### Hardware encoders ###

libcrystalhd_plugin_la_SOURCES = crystalhd.c h264_nal.h
libcrystalhd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libcrystalhd_plugin_la_LIBADD = $(LIBS_crystalhd)
EXTRA_LTLIBRARIES += libcrystalhd_plugin.la
codec_LTLIBRARIES += $(LTLIBcrystalhd)

libqsv_plugin_la_SOURCES = qsv.c
libqsv_plugin_la_CFLAGS = $(AM_CFLAGS) $(MFX_CFLAGS)
libqsv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdir)'
libqsv_plugin_la_LIBADD = $(MFX_LIBS)
EXTRA_LTLIBRARIES += libqsv_plugin.la
codec_LTLIBRARIES += $(LTLIBqsv)


### External frameworks ###

libdmo_plugin_la_SOURCES = dmo/dmo.c dmo/dmo.h dmo/buffer.c
libqsv_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libdmo_plugin_la_LIBADD = $(LIBM)
if HAVE_WIN32
libdmo_plugin_la_LIBADD += -lole32 -luuid
codec_LTLIBRARIES += libdmo_plugin.la
endif

libquicktime_plugin_la_SOURCES = quicktime.c
libquicktime_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(codecdirt)'
libquicktime_plugin_la_LIBADD = $(LIBM)
EXTRA_LTLIBRARIES += libquicktime_plugin.la
codec_LTLIBRARIES += $(LTLIBquicktime)
