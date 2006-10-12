COBJECTS = utils.o video-jni.o audio-jni.o input-jni.o playlist-jni.o vlm-jni.o core-jni.o graphics-jni.o

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

JAVACXXFLAGS = -I. -Isrc -I../../ -I ../../include -I../../../include  $(JINCLUDES) `top_builddir=../../.. ../../../vlc-config --cflags pic` -c


all: $(COBJECTS)

.cc.o:
	$(CXX) $? $(CXXFLAGS) $(JAVACXXFLAGS) -o $@

.PHONY: clean-local

clean-local:
	rm -f *.o *~

endif
