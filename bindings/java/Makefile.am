#######################################################################
# VLC Java Bindings
#######################################################################

if BUILD_JAVA

OBJECTS = org/videolan/jvlc/JVLCCanvas.class org/videolan/jvlc/JVLCPanel.class org/videolan/jvlc/AudioIntf.class org/videolan/jvlc/InputIntf.class org/videolan/jvlc/PlaylistIntf.class org/videolan/jvlc/VideoIntf.class org/videolan/jvlc/VLMIntf.class org/videolan/jvlc/Playlist.class org/videolan/jvlc/JLibVLC.class org/videolan/jvlc/JVLC.class org/videolan/jvlc/SWTVideoWidget.class

JNIHEADERS = org_videolan_jvlc_JVLC.h org_videolan_jvlc_JVLCCanvas.h org_videolan_jvlc_JVLCPanel.h

# Include some JAVA stuff
PROCESSOR_FAMILY = `uname -m | sed -e 's/^i.86/i386/' | sed -e 's/^x86_64/amd64/'`
JINCLUDES = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
LIBJINCLUDES = -L$(JAVA_HOME)/jre/lib/$(PROCESSOR_FAMILY) -ljawt
SWT_PATH = /usr/share/java


# Binaries
JCC = gcj -g -I${SWT_PATH}/swt-gtk.jar
JCH = gcjh -jni

# Compile flags
JAVACXXFLAGS = `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include $(JINCLUDES)
JAVALDFLAGS = -L../../lib -lvlc_pic `top_builddir=../.. ../../vlc-config --libs builtin vlc pic`

# Build targets
all: libjvlc.so VlcClient

libjvlc.so: vlc-libvlc-jni.o vlc-graphics-jni.o
	$(CXX) -shared vlc-libvlc-jni.o vlc-graphics-jni.o $(LIBJINCLUDES) $(LDFLAGS) $(JAVALDFLAGS) -o libjvlc.so

vlc-graphics-jni.o: VlcClient
	$(CXX) -c vlc-graphics-jni.cc $(CXXFLAGS) $(JAVACXXFLAGS)

vlc-libvlc-jni.o: VlcClient
	$(CXX) -c vlc-libvlc-jni.cc $(CXXFLAGS) $(JAVACXXFLAGS)

VlcClient: $(OBJECTS)
	$(JCC) -C VlcClient.java

%.class: %.java
	$(JCC) -C $?
	$(JCH) org/videolan/jvlc/$(*F)

clean:
	rm -f *.class *~ org/videolan/jvlc/*.class org_videolan*.h *.so *.o

endif
