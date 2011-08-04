NULL =
BASE_SUBDIRS = \
	access \
	audio_filter \
	audio_mixer \
	audio_output \
	codec \
	control \
	demux \
	gui \
	meta_engine \
	misc \
	media_library \
	packetizer \
	services_discovery \
	stream_filter \
	text_renderer \
	video_chroma \
	video_filter \
	video_output \
	visualization
EXTRA_SUBDIRS = \
	access_output \
	mux \
	stream_out \
	mmx \
	mmxext \
	sse2 \
	3dnow \
	altivec \
	arm_neon \
	lua \
	$(NULL)

SUBDIRS = $(BASE_SUBDIRS)
DIST_SUBDIRS = $(BASE_SUBDIRS) $(EXTRA_SUBDIRS) .
if ENABLE_SOUT
SUBDIRS += access_output mux stream_out
endif
if HAVE_MMX
SUBDIRS += mmx
endif
if HAVE_MMXEXT
SUBDIRS += mmxext
endif
if HAVE_SSE2
SUBDIRS += sse2
endif
if HAVE_3DNOW
SUBDIRS += 3dnow
endif
if HAVE_ALTIVEC
SUBDIRS += altivec
endif
if HAVE_NEON
SUBDIRS += arm_neon
endif
if BUILD_LUA
SUBDIRS += lua
endif
SUBDIRS += .

dist_noinst_SCRIPTS = genmf list.sh
dist_noinst_DATA = LIST
MOSTLYCLEANFILES = plugins.dat*

.PHONY: stamp-cache

stamp-cache:
	$(AM_V_GEN)if test "$(build)" = "$(host)"; then \
		../bin/vlc-cache-gen$(EXEEXT) . ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi

install-exec-hook:
	if test -z "$(DESTDIR)" -a "$(build)" = "$(host)"; then \
		LD_LIBRARY_PATH="$(libdir)" \
		"$(vlclibdir)/vlc-cache-gen$(EXEEXT)" \
			 "$(vlclibdir)/plugins" ; \
	else \
		echo "Staged installation: cache generation skipped!" ; \
	fi
