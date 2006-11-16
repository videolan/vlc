#######################################################################
# VLC Java Bindings
#######################################################################

SUBDIRS= . src

EXTRA_DIST= \
	    FAQ \
	    TODO \
	    README \
	    THANKS \
	    javadoc.xml \
	    VlcClient.java \
	    VLCExample.java

EXTRA_DIST+= \
	org/videolan/jvlc/AudioIntf.java \
	org/videolan/jvlc/Audio.java \
	org/videolan/jvlc/GenericVideoWidget.java \
	org/videolan/jvlc/InputIntf.java \
	org/videolan/jvlc/Input.java \
	org/videolan/jvlc/JLibVLC.java \
	org/videolan/jvlc/JVLCCanvas.java \
	org/videolan/jvlc/JVLC.java \
	org/videolan/jvlc/JVLCPanel.java \
	org/videolan/jvlc/PlaylistIntf.java \
	org/videolan/jvlc/Playlist.java \
	org/videolan/jvlc/VideoIntf.java \
	org/videolan/jvlc/Video.java \
	org/videolan/jvlc/VLCException.java \
	org/videolan/jvlc/VLMIntf.java \
	org/videolan/jvlc/VLM.java 

if BUILD_JAVA

OBJECTS = org/videolan/jvlc/VLCException.class org/videolan/jvlc/Playlist.class org/videolan/jvlc/AudioIntf.class org/videolan/jvlc/Audio.class org/videolan/jvlc/InputIntf.class org/videolan/jvlc/Input.class org/videolan/jvlc/PlaylistIntf.class org/videolan/jvlc/VideoIntf.class org/videolan/jvlc/Video.class org/videolan/jvlc/JLibVLC.class org/videolan/jvlc/JVLC.class org/videolan/jvlc/JVLCCanvas.class org/videolan/jvlc/JVLCPanel.class org/videolan/jvlc/VLMIntf.class org/videolan/jvlc/VLM.class  org/videolan/jvlc/GenericVideoWidget.class

PROCESSOR_FAMILY = `uname -m | sed -e 's/^i.86/i386/' | sed -e 's/^x86_64/amd64/'`

if HAVE_WIN32
JINCLUDES = -I${JAVA_HOME}/include -I${JAVA_HOME}/include/win32
LIBJINCLUDES = -L${JAVA_HOME}/lib -ljawt
JCH = javah
else
JINCLUDES = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
LIBJINCLUDES = -L$(JAVA_HOME)/jre/lib/$(PROCESSOR_FAMILY) -ljawt
JCH = gcjh -jni
endif

export JINCLUDES
export LIBJINCLUDES

# Build targets

if HAVE_WIN32
%.class: %.java 
	$(JAVAC) $? 
	$(JCH) org.videolan.jvlc.$(*F)
else
%.class: %.java
	$(JAVAC) $?
	$(JCH) $(@D)/$(*F) -o includes/$(*F).h
endif

VlcClient: $(OBJECTS)
	$(JAVAC) VlcClient.java

VLCExample: $(OBJECTS)
	$(JAVAC) VLCExample.java

all-am: VlcClient VLCExample

clean-local:
	rm -f *.class *~ org/videolan/jvlc/*.class org_videolan*.h includes/*.h src/*.o *.so *.o *.dll

endif
