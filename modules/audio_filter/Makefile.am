audio_filterdir = $(pluginsdir)/audio_filter

libaudiobargraph_a_plugin_la_SOURCES = audio_filter/audiobargraph_a.c
libaudiobargraph_a_plugin_la_LIBADD = $(LIBM)
libchorus_flanger_plugin_la_SOURCES = audio_filter/chorus_flanger.c
libchorus_flanger_plugin_la_LIBADD = $(LIBM)
libcompressor_plugin_la_SOURCES = audio_filter/compressor.c
libcompressor_plugin_la_LIBADD = $(LIBM)
libequalizer_plugin_la_SOURCES = audio_filter/equalizer.c \
	audio_filter/equalizer_presets.h
libequalizer_plugin_la_LIBADD = $(LIBM)
libkaraoke_plugin_la_SOURCES = audio_filter/karaoke.c
libnormvol_plugin_la_SOURCES = audio_filter/normvol.c
libnormvol_plugin_la_LIBADD = $(LIBM)
libgain_plugin_la_SOURCES = audio_filter/gain.c
libparam_eq_plugin_la_SOURCES = audio_filter/param_eq.c
libparam_eq_plugin_la_LIBADD = $(LIBM)
libscaletempo_plugin_la_SOURCES = audio_filter/scaletempo.c
libscaletempo_plugin_la_LIBADD = $(LIBM)
libscaletempo_pitch_plugin_la_SOURCES = $(libscaletempo_plugin_la_SOURCES)
libscaletempo_pitch_plugin_la_LIBADD = $(libscaletempo_plugin_la_LIBADD)
libscaletempo_pitch_plugin_la_CFLAGS = $(AM_CFLAGS) -DPITCH_SHIFTER
libstereo_widen_plugin_la_SOURCES = audio_filter/stereo_widen.c
libspatializer_plugin_la_SOURCES = \
	audio_filter/spatializer/allpass.cpp \
	audio_filter/spatializer/allpass.hpp \
	audio_filter/spatializer/comb.cpp \
	audio_filter/spatializer/comb.hpp \
	audio_filter/spatializer/denormals.h \
	audio_filter/spatializer/denormals.c \
	audio_filter/spatializer/tuning.h \
	audio_filter/spatializer/revmodel.cpp \
	audio_filter/spatializer/revmodel.hpp \
	audio_filter/spatializer/spatializer.cpp
libspatializer_plugin_la_LIBADD = $(LIBM)
libcenter_plugin_la_SOURCES = audio_filter/center.c
libcenter_plugin_la_LIBADD  = $(LIBM)

audio_filter_LTLIBRARIES = \
	libaudiobargraph_a_plugin.la \
	libchorus_flanger_plugin.la \
	libcompressor_plugin.la \
	libequalizer_plugin.la \
	libkaraoke_plugin.la \
	libnormvol_plugin.la \
	libgain_plugin.la \
	libparam_eq_plugin.la \
	libscaletempo_plugin.la \
	libscaletempo_pitch_plugin.la \
	libspatializer_plugin.la \
	libstereo_widen_plugin.la \
	libcenter_plugin.la

# Channel mixers
libdolby_surround_decoder_plugin_la_SOURCES = \
	audio_filter/channel_mixer/dolby.c
libheadphone_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/headphone.c
libheadphone_channel_mixer_plugin_la_LIBADD = $(LIBM)
libmono_plugin_la_SOURCES = audio_filter/channel_mixer/mono.c
libmono_plugin_la_LIBADD = $(LIBM)
libremap_plugin_la_SOURCES = audio_filter/channel_mixer/remap.c
libtrivial_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/trivial.c
libsimple_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/simple.c
libsimple_channel_mixer_plugin_la_CFLAGS =
libsimple_channel_mixer_plugin_la_LIBADD =

if HAVE_NEON
EXTRA_LTLIBRARIES += libsimple_channel_mixer_plugin_arm_neon.la
libsimple_channel_mixer_plugin_arm_neon_la_SOURCES = arm_neon/simple_channel_mixer.S
# Intentionally leaving out AM_LDFLAGS from this one; it's not meant to be
# built like a plugin.
libsimple_channel_mixer_plugin_arm_neon_la_LDFLAGS = -static

libsimple_channel_mixer_plugin_la_LIBADD += libsimple_channel_mixer_plugin_arm_neon.la
libsimple_channel_mixer_plugin_la_CFLAGS += -DCAN_COMPILE_NEON
libsimple_channel_mixer_plugin_la_SOURCES += \
	audio_filter/channel_mixer/simple_neon.h
endif

audio_filter_LTLIBRARIES += \
	libdolby_surround_decoder_plugin.la \
	libheadphone_channel_mixer_plugin.la \
	libmono_plugin.la \
	libremap_plugin.la \
	libsimple_channel_mixer_plugin.la \
	libtrivial_channel_mixer_plugin.la

# Spatial audio: ambisonics / binaural
libspatialaudio_plugin_la_SOURCES = \
	audio_filter/channel_mixer/spatialaudio.cpp
libspatialaudio_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(SPATIALAUDIO_CFLAGS)
libspatialaudio_plugin_la_LIBADD = $(SPATIALAUDIO_LIBS)
libspatialaudio_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
EXTRA_LTLIBRARIES += libspatialaudio_plugin.la
audio_filter_LTLIBRARIES += $(LTLIBspatialaudio)

# Converters
libaudio_format_plugin_la_SOURCES = audio_filter/converter/format.c
libaudio_format_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libaudio_format_plugin_la_LIBADD = $(LIBM)

libtospdif_plugin_la_SOURCES = audio_filter/converter/tospdif.c \
	packetizer/a52.h \
	packetizer/dts_header.c packetizer/dts_header.h

audio_filter_LTLIBRARIES += \
	libtospdif_plugin.la \
	libaudio_format_plugin.la

# Resamplers
libbandlimited_resampler_plugin_la_SOURCES = \
	audio_filter/resampler/bandlimited.c \
	audio_filter/resampler/bandlimited.h
libugly_resampler_plugin_la_SOURCES = audio_filter/resampler/ugly.c
libsamplerate_plugin_la_SOURCES = audio_filter/resampler/src.c
libsamplerate_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SAMPLERATE_CFLAGS)
libsamplerate_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
libsamplerate_plugin_la_LIBADD = $(SAMPLERATE_LIBS) $(LIBM)

libsoxr_plugin_la_SOURCES = audio_filter/resampler/soxr.c
libsoxr_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SOXR_CFLAGS)
libsoxr_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
libsoxr_plugin_la_LIBADD = $(SOXR_LIBS) $(LIBM)

audio_filter_LTLIBRARIES += \
	$(LTLIBsamplerate) \
	$(LTLIBsoxr) \
	libugly_resampler_plugin.la
EXTRA_LTLIBRARIES += \
	libbandlimited_resampler_plugin.la \
	libsamplerate_plugin.la \
	libsoxr_plugin.la

libspeex_resampler_plugin_la_SOURCES = audio_filter/resampler/speex.c
libspeex_resampler_plugin_la_CFLAGS = $(AM_CFLAGS) $(SPEEXDSP_CFLAGS)
libspeex_resampler_plugin_la_LIBADD = $(SPEEXDSP_LIBS)
if HAVE_SPEEXDSP
audio_filter_LTLIBRARIES += libspeex_resampler_plugin.la
endif
