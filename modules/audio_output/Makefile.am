aoutdir = $(pluginsdir)/audio_output
aout_LTLIBRARIES =

libopensles_android_plugin_la_SOURCES = audio_output/opensles_android.c
libopensles_android_plugin_la_LIBADD = $(LIBDL) $(LIBM)

libandroid_audiotrack_plugin_la_SOURCES = audio_output/audiotrack.c \
	video_output/android/utils.c video_output/android/utils.h
libandroid_audiotrack_plugin_la_CFLAGS = $(AM_CFLAGS)

if HAVE_ANDROID
aout_LTLIBRARIES += libandroid_audiotrack_plugin.la libopensles_android_plugin.la
endif

libadummy_plugin_la_SOURCES = audio_output/adummy.c

libafile_plugin_la_SOURCES = audio_output/file.c
libafile_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)

libamem_plugin_la_SOURCES = audio_output/amem.c

aout_LTLIBRARIES += \
	libadummy_plugin.la \
	libafile_plugin.la \
	libamem_plugin.la

liboss_plugin_la_SOURCES = audio_output/oss.c audio_output/volume.h
liboss_plugin_la_LIBADD = $(OSS_LIBS) $(LIBM)
if HAVE_OSS
aout_LTLIBRARIES += liboss_plugin.la
endif

libalsa_plugin_la_SOURCES = audio_output/alsa.c audio_output/volume.h
libalsa_plugin_la_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS)
libalsa_plugin_la_LIBADD = $(ALSA_LIBS) $(LIBM)
if HAVE_ALSA
aout_LTLIBRARIES += libalsa_plugin.la
endif

libvlc_pulse_la_SOURCES = audio_output/vlcpulse.c audio_output/vlcpulse.h
libvlc_pulse_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libvlc_pulse_la_LIBADD = $(PULSE_LIBS) $(LTLIBVLCCORE)
libvlc_pulse_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^vlc_pa_ \
	-version-info 0:0:0
libpulse_plugin_la_SOURCES = audio_output/pulse.c
libpulse_plugin_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulse_plugin_la_LIBADD = libvlc_pulse.la $(PULSE_LIBS) $(LIBM)
if HAVE_PULSE
pkglib_LTLIBRARIES += libvlc_pulse.la
aout_LTLIBRARIES += libpulse_plugin.la
endif

libjack_plugin_la_SOURCES = audio_output/jack.c audio_output/volume.h
libjack_plugin_la_CFLAGS = $(AM_CFLAGS) $(JACK_CFLAGS)
libjack_plugin_la_LIBADD = $(JACK_LIBS) $(LIBM)
if HAVE_JACK
aout_LTLIBRARIES += libjack_plugin.la
endif

libmmdevice_plugin_la_SOURCES = audio_output/mmdevice.c audio_output/mmdevice.h
libmmdevice_plugin_la_LIBADD = $(LIBCOM) $(LIBM)
libwinstore_plugin_la_SOURCES = audio_output/winstore.c audio_output/mmdevice.h
libwinstore_plugin_la_LIBADD = $(LIBCOM) -lmmdevapi
libwasapi_plugin_la_SOURCES = audio_output/wasapi.c
libwasapi_plugin_la_LIBADD = $(LIBCOM) -lksuser
if HAVE_WASAPI
aout_LTLIBRARIES += libwasapi_plugin.la
if !HAVE_WINSTORE
aout_LTLIBRARIES += libmmdevice_plugin.la
else
aout_LTLIBRARIES += libwinstore_plugin.la
endif
endif

libdirectsound_plugin_la_SOURCES = audio_output/directsound.c \
	audio_output/windows_audio_common.h
libdirectsound_plugin_la_LIBADD = -ldsound $(LIBCOM)
if HAVE_WIN32_DESKTOP
aout_LTLIBRARIES += libdirectsound_plugin.la
endif

libkai_plugin_la_SOURCES = audio_output/kai.c
libkai_plugin_la_LIBADD = $(KAI_LIBS)
if HAVE_KAI
aout_LTLIBRARIES += libkai_plugin.la
endif

libsndio_plugin_la_SOURCES = audio_output/sndio.c
libsndio_plugin_la_LIBADD = -lsndio $(LIBM)
if HAVE_SNDIO
aout_LTLIBRARIES += libsndio_plugin.la
endif

libwaveout_plugin_la_SOURCES = audio_output/waveout.c \
	audio_output/windows_audio_common.h
libwaveout_plugin_la_LIBADD = -lwinmm
if HAVE_WIN32_DESKTOP
aout_LTLIBRARIES += libwaveout_plugin.la
endif

libauhal_plugin_la_SOURCES = audio_output/auhal.c \
	audio_output/coreaudio_common.c audio_output/coreaudio_common.h
libauhal_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(aoutdir)' \
	-Wl,-framework,CoreAudio,-framework,AudioUnit,-framework,AudioToolbox,-framework,CoreServices
if HAVE_OSX
aout_LTLIBRARIES += libauhal_plugin.la
endif
libaudiounit_ios_plugin_la_SOURCES = audio_output/audiounit_ios.m \
	audio_output/coreaudio_common.c audio_output/coreaudio_common.h
libaudiounit_ios_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(aoutdir)' \
	-Wl,-framework,Foundation,-framework,CoreAudio,-framework,AudioToolbox,-framework,UIKit,-framework,AVFoundation
if HAVE_IOS
aout_LTLIBRARIES += libaudiounit_ios_plugin.la
endif
if HAVE_TVOS
aout_LTLIBRARIES += libaudiounit_ios_plugin.la
endif
