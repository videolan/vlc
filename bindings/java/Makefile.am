#######################################################################
# VLC Java Bindings
#######################################################################

if BUILD_JAVA

OBJECTS = org/videolan/jvlc/VLCException.class org/videolan/jvlc/Playlist.class org/videolan/jvlc/AudioIntf.class org/videolan/jvlc/Audio.class org/videolan/jvlc/InputIntf.class org/videolan/jvlc/Input.class org/videolan/jvlc/PlaylistIntf.class org/videolan/jvlc/VideoIntf.class org/videolan/jvlc/Video.class org/videolan/jvlc/JLibVLC.class org/videolan/jvlc/JVLC.class org/videolan/jvlc/JVLCCanvas.class org/videolan/jvlc/JVLCPanel.class org/videolan/jvlc/VLMIntf.class org/videolan/jvlc/VLM.class  org/videolan/jvlc/GenericVideoWidget.class

COBJECTS = src/utils.o src/video-jni.o src/audio-jni.o src/input-jni.o src/playlist-jni.o src/vlm-jni.o src/core-jni.o src/graphics-jni.o

# Include some JAVA stuff
PROCESSOR_FAMILY = `uname -m | sed -e 's/^i.86/i386/' | sed -e 's/^x86_64/amd64/'`

if HAVE_WIN32
JINCLUDES = -I${JAVA_HOME}/include -I${JAVA_HOME}/include/win32
LIBJINCLUDES = -L${JAVA_HOME}/lib -ljawt
JCH = javah
JAVACXXFLAGS = `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES)
JAVALDFLAGS = -mno-cygwin -L../../src -lvlc `top_builddir=../.. ../../vlc-config --libs builtin vlc pic` -Wl,--kill-at
else
JINCLUDES = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
LIBJINCLUDES = -L$(JAVA_HOME)/jre/lib/$(PROCESSOR_FAMILY) -ljawt
JCH = gcjh -jni
JAVACXXFLAGS = `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES)
JAVALDFLAGS = -L../../src -lvlc `top_builddir=../.. ../../vlc-config --libs builtin vlc pic`
endif

export JINCLUDES

# Build targets
all: libjvlc.so VlcClient VLCExample

if HAVE_WIN32
libjvlc.so: $(OBJECTS)
	$(MAKE) -C src
	$(CXX) -shared $(COBJECTS) $(LIBJINCLUDES) $(LDFLAGS) $(JAVALDFLAGS) -o jvlc.dll
else
libjvlc.so: $(OBJECTS) 	
	$(MAKE) -C src
	$(CXX) -shared $(COBJECTS) $(LIBJINCLUDES) $(LDFLAGS) $(JAVALDFLAGS) -o libjvlc.so
endif

VlcClient: $(OBJECTS)
	$(JAVAC) VlcClient.java

VLCExample: $(OBJECTS)
	$(JAVAC) VLCExample.java

if HAVE_WIN32
%.class: %.java
	$(JAVAC)  $?
	$(JCH) org.videolan.jvlc.$(*F) -o includes/$(*F).h
else
%.class: %.java
	$(JAVAC) $?
	$(JCH) org/videolan/jvlc/$(*F) -o includes/$(*F).h
endif

clean:
	rm -f *.class *~ org/videolan/jvlc/*.class org_videolan*.h *.so *.o *.dll
	$(MAKE) -C src clean-local

endif
