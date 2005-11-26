#######################################################################
# VLC Java Bindings
#######################################################################

if BUILD_JAVA

OBJECTS = org/videolan/jvlc/JVLCNoSuchVariableNameException.class org/videolan/jvlc/JVLCBoolVariable.class org/videolan/jvlc/JVLCIntVariable.class org/videolan/jvlc/JVLCTimeVariable.class org/videolan/jvlc/JVLCVarValue.class org/videolan/jvlc/JVLCVariable.class org/videolan/jvlc/JVLCFloatVariable.class org/videolan/jvlc/JVLCStringVariable.class org/videolan/jvlc/JVLCVarVariable.class org/videolan/jvlc/JVLC.class org/videolan/jvlc/JVLCCanvas.class

JNIHEADERS = org/videolan/jvlc/JVLCNoSuchVariableNameException.h org_videolan_jvlc_JVLCBoolVariable.h org_videolan_jvlc_JVLCFloatVariable.h org_videolan_jvlc_JVLC.h org_videolan_jvlc_JVLCIntVariable.h org_videolan_jvlc_JVLCStringVariable.h org_videolan_jvlc_JVLCTimeVariable.h org_videolan_jvlc_JVLCVariable.h org_videolan_jvlc_JVLCVarValue.h org_videolan_jvlc_JVLCVarVariable.h org_videolan_jvlc_JVLCCanvas.h

# Include some JAVA stuff
JINCLUDES = -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux
LIBJINCLUDES = -L${JAVA_HOME}/jre/lib/i386 -ljawt

# Binaries
JCC = gcj -g
JCH = gcjh -jni

# Compile flags
CXXFLAGS += `top_builddir=../.. ../../vlc-config --cflags pic` -I../../ -I ../../include ${JINCLUDES}
LDFLAGS += -L../../lib -lvlc_pic `top_builddir=../.. ../../vlc-config --libs builtin vlc pic`

# Build targets
all: libjvlc.so VlcClient

libjvlc.so: vlc-glue-jni.o vlc-graphics-jni.o
	$(CXX) -shared vlc-glue-jni.o vlc-graphics-jni.o $(LIBJINCLUDES) ${LDFLAGS} -o libjvlc.so

vlc-graphics-jni.o: VlcClient
	$(CXX) -c vlc-graphics-jni.cc $(CXXFLAGS)

vlc-glue-jni.o: VlcClient
	$(CXX) -c vlc-glue-jni.cc $(CXXFLAGS)

VlcClient: $(OBJECTS)
	$(JCC) -C VlcClient.java

%.class: %.java
	$(JCC) -C $?
	$(JCH) org/videolan/jvlc/$(*F)

clean:
	rm -f *.class *~ org/videolan/jvlc/*.class org_videolan*.h *.so *.o

endif
