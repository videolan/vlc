EXTRA_DIST= \
	audio-jni.cc \
	core-jni.cc \
	graphics-jni.cc \
	input-jni.cc \
	playlist-jni.cc \
	utils.cc \
	utils.h \
	video-jni.cc \
	vlm-jni.cc

if BUILD_JAVA

if USE_LIBTOOL

jvlcdir = $(libdir)
jvlc_LTLIBRARIES = libjvlc.la
libjvlc_la_SOURCES = \
	audio-jni.cc \
	core-jni.cc \
	graphics-jni.cc \
	input-jni.cc \
	playlist-jni.cc \
	utils.cc \
	utils.h \
	video-jni.cc \
	vlm-jni.cc

libjvlc_la_LIBADD = ../../../src/libvlc-control.la $(LIBJINCLUDES)

else

COBJECTS = utils.o video-jni.o audio-jni.o input-jni.o playlist-jni.o vlm-jni.o core-jni.o graphics-jni.o

JAVACXXFLAGS = -I. -Isrc -I../../ -I ../../include -I../../../include  $(JINCLUDES) `top_builddir=../../.. ../../../vlc-config --cflags pic` -c

.cc.o:
	$(CXX) $? $(CXXFLAGS) $(JAVACXXFLAGS) -o $@

all-am: $(COBJECTS)

endif

.PHONY: clean-local

clean-local:
	rm -f *.o *~

endif
