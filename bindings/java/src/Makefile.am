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
LIBRARIES_libvlc = $(top_builddir)/src/libvlc.dll.a
libjvlc_a_DEPENDENCIES = libjvlc.def $(LIBRARIES_libvlc)
LDFLAGS_jvlc = `$(VLC_CONFIG) --libs libvlc`
libjvlc_DATA = libjvlc$(LIBEXT)

libjvlcdir=$(srcdir)

#libjvlc.dll.a libjvlc.def: libjvlc$(LIBEXT)
#	$(DLLTOOL) -z libjvlc.def -l libjvlc.dll.a -D $< $(libjvlc_a_OBJECTS)

libjvlc$(LIBEXT): $(libjvlc_a_OBJECTS) \
                      $(libjvlc_a_DEPENDENCIES)
		  $(CXXLINK) $(libjvlc_a_OBJECTS) \
		        $(srcdir)/libjvlc.def \
		        $(LIBRARIES_libvlc) -shared $(LIBRARIES_libvlc) $(LIBJINCLUDES) $(LDFLAGS_jvlc) \
		        $(INCLUDED_LIBINTL)
# Cygwin work-around, copied by other makefiles
			@if test -f "$@.exe"; then mv -f "$@.exe" "$@"; fi


all: libjvlc.dll.a
endif

.PHONY: clean-local

clean-local:
	rm -f *.o *~

endif
