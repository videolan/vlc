OBJECTS = VlcClient.o JVLC.o JVLCVariable.o JVLCVarValue.o JVLCVarVariable.o JVLCIntVariable.o JVLCTimeVariable.o JVLCStringVariable.o JVLCFloatVariable.o JVLCBoolVariable.o vlc-glue.o
FLAGS = -I. -I/usr/local/include/vlc -L../../lib -lvlc `top_builddir=../.. ../../vlc-config --libs vlc builtin extern` `top_builddir=../.. ../../vlc-config --cxxflags` --main=VlcClient
JHEADERS = JVLC.h JVLCVariable.h JVLCIntVariable.h JVLCBoolVariable.h JVLCTimeVariable.h JVLCStringVariable.h JVLCFloatVariable.h JVLCVarVariable.h JVLCVarValue.h
JCC=gcj
CXX=g++
JCH=gcjh
DEBUG=-g

VlcClient: $(OBJECTS)
	$(JCC) -o jvlc -I. $(OBJECTS) $(DEBUG) $(FLAGS)

VlcClient.class: VlcClient.java
	$(JCC) $(DEBUG) -C VlcClient.java

%.o: %.class
	$(JCC) $(DEBUG) -c $?

%.class: %.java
	$(JCC) $(DEBUG) -C $?

%.h: %.class
	$(JCH) $(*F)

vlc-glue.o: $(JHEADERS) vlc-glue.cc
	$(CXX) $(DEBUG) -I./ -c vlc-glue.cc

clean:
	rm -f jvlc *.o *.class $(JHEADERS) *~
