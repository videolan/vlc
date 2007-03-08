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
libjvlc_la_CPPFLAGS = `$(VLC_CONFIG) --cflags pic` $(JINCLUDES)
libjvlc_la_LIBADD = ../../../src/libvlc-control.la $(LIBJINCLUDES)

libjvlc_a_SOURCES=$(libjvlc_la_SOURCES)
libjvlc_a_CPPFLAGS = `$(VLC_CONFIG) --cflags pic` $(JINCLUDES)
libjvlc_a_LIBADD = ../../../src/libvlc.a


if BUILD_JAVA

if USE_LIBTOOL
lib_LTLIBRARIES = libjvlc.la
else
lib_LIBRARIES = libjvlc.a
endif 

if HAVE_WIN32
DATA_noinst_libjvlc = libjvlc$(LIBEXT) libjvlc$(LIBEXT).a

libjvlc.dll.a libjvlc.def: libjvlc$(LIBEXT)
	$(DLLTOOL) -z libjvlc.def -l libjvlc.dll.a -D $< $(libjvlc_a_OBJECTS)

libjvlc$(LIBEXT): $(libjvlc_a_OBJECTS)

all: libjvlc.dll.a
endif

.PHONY: clean-local

clean-local:
	rm -f *.o *~

endif
