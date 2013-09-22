aoutdir = $(pluginsdir)/audio_output
aout_LTLIBRARIES =
EXTRA_LTLIBRARIES =

include ../common.am

libopensles_android_plugin_la_SOURCES = opensles_android.c
libopensles_android_plugin_la_LIBADD = $(LIBDL) $(LIBM)

libandroid_audiotrack_plugin_la_SOURCES = audiotrack.c
libandroid_audiotrack_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libandroid_audiotrack_plugin_la_LIBADD = $(LIBDL)

if HAVE_ANDROID
aout_LTLIBRARIES += libandroid_audiotrack_plugin.la libopensles_android_plugin.la
endif

libadummy_plugin_la_SOURCES = adummy.c

libafile_plugin_la_SOURCES = file.c
libafile_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libamem_plugin_la_SOURCES = amem.c

aout_LTLIBRARIES += \
	libadummy_plugin.la \
	libafile_plugin.la \
	libamem_plugin.la

liboss_plugin_la_SOURCES = oss.c volume.h
liboss_plugin_la_LIBADD = $(OSS_LIBS) $(LIBM)
if HAVE_OSS
aout_LTLIBRARIES += liboss_plugin.la
endif

libalsa_plugin_la_SOURCES = alsa.c volume.h
libalsa_plugin_la_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS)
libalsa_plugin_la_LIBADD = $(ALSA_LIBS) $(LIBM)
if HAVE_ALSA
aout_LTLIBRARIES += libalsa_plugin.la
endif

libpulse_plugin_la_SOURCES = pulse.c
libpulse_plugin_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulse_plugin_la_LIBADD = ../libvlc_pulse.la $(PULSE_LIBS) $(LIBM)
if HAVE_PULSE
aout_LTLIBRARIES += libpulse_plugin.la
endif

libjack_plugin_la_SOURCES = jack.c volume.h
libjack_plugin_la_CFLAGS = $(AM_CFLAGS) $(JACK_CFLAGS)
libjack_plugin_la_LIBADD = $(JACK_LIBS) $(LIBM)
if HAVE_JACK
aout_LTLIBRARIES += libjack_plugin.la
endif

libmmdevice_plugin_la_SOURCES = mmdevice.c mmdevice.h wasapi.c
libmmdevice_plugin_la_LIBADD = -lole32 -lksuser
if HAVE_WASAPI
aout_LTLIBRARIES += libmmdevice_plugin.la
endif

libdirectsound_plugin_la_SOURCES = directsound.c windows_audio_common.h
libdirectsound_plugin_la_LIBADD = -lole32
if HAVE_DIRECTX
aout_LTLIBRARIES += libdirectsound_plugin.la
endif

libkai_plugin_la_SOURCES = kai.c
libkai_plugin_la_LIBADD = $(KAI_LIBS)
if HAVE_KAI
aout_LTLIBRARIES += libkai_plugin.la
endif

libsndio_plugin_la_SOURCES = sndio.c
libsndio_plugin_la_LIBADD = -lsndio
if HAVE_SNDIO
aout_LTLIBRARIES += libsndio_plugin.la
endif

libwaveout_plugin_la_SOURCES = waveout.c windows_audio_common.h
libwaveout_plugin_la_LIBADD = -lwinmm
if HAVE_WIN32
aout_LTLIBRARIES += libwaveout_plugin.la
endif

libauhal_plugin_la_SOURCES = TPCircularBuffer.h TPCircularBuffer.c auhal.c
libauhal_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,CoreAudio,-framework,AudioUnit,-framework,AudioToolbox,-framework,CoreServices -rpath '$(aoutdir)'
libaudiounit_ios_plugin_la_SOURCES = TPCircularBuffer.h TPCircularBuffer.c audiounit_ios.c
libaudiounit_ios_plugin_la_LDFLAGS = $(libauhal_plugin_la_LDFLAGS)
libaudioqueue_plugin_la_SOURCES = audioqueue.c
libaudioqueue_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,CoreAudio,-framework,AudioUnit,-framework,AudioToolbox,-framework,CoreServices -rpath '$(aoutdir)'
EXTRA_LTLIBRARIES += libauhal_plugin.la libaudiounit_ios_plugin.la libaudioqueue_plugin.la
aout_LTLIBRARIES += $(LTLIBauhal) $(LTLIBaudiounit_ios) $(LTLIBaudioqueue)
