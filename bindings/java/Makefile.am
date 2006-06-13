#######################################################################
# VLC Java Bindings
#######################################################################

if BUILD_JAVA

OBJECTS = org/videolan/jvlc/AudioIntf.class org/videolan/jvlc/InputIntf.class org/videolan/jvlc/PlaylistIntf.class org/videolan/jvlc/VideoIntf.class org/videolan/jvlc/JLibVLC.class org/videolan/jvlc/JVLC.class org/videolan/jvlc/JVLCCanvas.class org/videolan/jvlc/JVLCPanel.class org/videolan/jvlc/VLMIntf.class org/videolan/jvlc/Playlist.class org/videolan/jvlc/GenericVideoWidget.class

# Include some JAVA stuff
PROCESSOR_FAMILY = `uname -m | sed -e 's/^i.86/i386/' | sed -e 's/^x86_64/amd64/'`
if HAVE_WIN32
JINCLUDES = -I${JAVA_HOME}/include -I${JAVA_HOME}/include/win32
LIBJINCLUDES = -L${JAVA_HOME}/lib -ljawt
JCC = javac
JCH = javah
JAVA_CC_FLAGS =
#CXX = g++ -Wall -mno-cygwin -g
#LDFLAGS = -mno-cygwin `vlc-config --libs external pic` -Wl,--kill-at
JAVACXXFLAGS = `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES)
JAVALDFLAGS = -mno-cygwin -L../../src -lvlc `top_builddir=../.. ../../vlc-config --libs builtin vlc pic` -Wl,--kill-at
else
JINCLUDES = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
LIBJINCLUDES = -L$(JAVA_HOME)/jre/lib/$(PROCESSOR_FAMILY) -ljawt
JCC = gcj -g
JCH = gcjh -jni
SWT_PATH = /usr/share/java
# Compile flags
JAVA_CC_FLAGS = -C
JAVACXXFLAGS = `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES)
JAVALDFLAGS = -L../../src -lvlc `top_builddir=../.. ../../vlc-config --libs builtin vlc pic`
endif

# Build targets
all: libjvlc.so VlcClient VLCExample

if HAVE_WIN32
libjvlc.so: vlc-libvlc-jni.o vlc-graphics-jni.o
	$(CXX) -shared vlc-libvlc-jni.o vlc-graphics-jni.o $(LIBJINCLUDES) $(LDFLAGS) $(JAVALDFLAGS) -o jvlc.dll
else
libjvlc.so: vlc-libvlc-jni.o vlc-graphics-jni.o
	$(CXX) -shared vlc-libvlc-jni.o vlc-graphics-jni.o $(LIBJINCLUDES) $(LDFLAGS) $(JAVALDFLAGS) -o libjvlc.so
endif

vlc-graphics-jni.o: VlcClient
	$(CXX) -c vlc-graphics-jni.cc $(CXXFLAGS) $(JAVACXXFLAGS)

vlc-libvlc-jni.o: VlcClient
	$(CXX) -c vlc-libvlc-jni.cc $(CXXFLAGS) $(JAVACXXFLAGS)

VlcClient: $(OBJECTS)
	$(JCC) $(JAVA_CC_FLAGS) VlcClient.java

VLCExample: $(OBJECTS)
	$(JCC) $(JAVA_CC_FLAGS) VLCExample.java

if HAVE_WIN32
%.class: %.java
	$(JCC) $(JAVA_CC_FLAGS) $?
	$(JCH) org.videolan.jvlc.$(*F)
else
%.class: %.java
	$(JCC) $(JAVA_CC_FLAGS) $?
	$(JCH) org/videolan/jvlc/$(*F)

clean:
	rm -f *.class *~ org/videolan/jvlc/*.class org_videolan*.h *.so *.o

endif
