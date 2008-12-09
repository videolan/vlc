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
	stream_out

SUBDIRS = $(BASE_SUBDIRS)
DIST_SUBDIRS = $(BASE_SUBDIRS) $(EXTRA_SUBDIRS)
if ENABLE_SOUT
SUBDIRS += access_output mux stream_out
endif

dist_noinst_SCRIPTS = genmf list.sh
dist_noinst_DATA = LIST
