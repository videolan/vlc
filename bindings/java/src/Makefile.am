if BUILD_JAVA

COBJECTS = utils.o video-jni.o audio-jni.o input-jni.o playlist-jni.o vlm-jni.o core-jni.o graphics-jni.o
JAVACXXFLAGS = `top_builddir=../../.. ../../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES) -c


all: $(COBJECTS)

.cc.o:
	$(CXX) $? $(CXXFLAGS) $(JAVACXXFLAGS) -o $@

.PHONY: clean-local

clean-local:
	rm -f *.o *~

endif
