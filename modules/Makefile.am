SUBDIRS = \
	access \
	access_filter \
	access_output \
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
	video_chroma \
	video_filter \
	video_output \
	visualization
EXTRA_SUBDIRS = \
	mux \
	stream_out
if ENABLE_SOUT
SUBDIRS += mux stream_out
endif

DIST_SUBDIRS = $(SUBDIRS) $(EXTRA_SUBDIRS)

dist_noinst_SCRIPTS = genmf list.sh
dist_noinst_DATA = LIST
