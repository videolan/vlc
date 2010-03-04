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
	packetizer \
	services_discovery \
	stream_filter \
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
if HAVE_ARM_NEON
SUBDIRS += arm_neon
endif
SUBDIRS += .

dist_noinst_SCRIPTS = genmf list.sh
dist_noinst_DATA = LIST
noinst_DATA = stamp-cache
MOSTLYCLEANFILES = stamp-cache plugins-*-*.dat

.PHONY: stamp-cache

stamp-cache:
	$(AM_V_GEN)if test "$(build)" = "$(host)"; then \
		../bin/vlc-cache-gen$(EXEEXT) . ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi
	$(AM_V_at)touch stamp-cache

install-exec-hook:
	if test -z "$(DESTDIR)"; then \
		../bin/vlc-cache-gen$(EXEEXT) "$(vlclibdir)/plugins" ; \
	else \
		echo "Staged installation: cache generation skipped!" ; \
	fi
