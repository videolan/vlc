libjvlc_la_SOURCES = \
	audio-jni.cc \
	core-jni.cc \
	graphics-jni.cc \
	input-jni.cc \
	playlist-jni.cc \
	utils.cc \
	utils.h \
	video-jni.cc \
	vlm-jni.cc \
	callback-jni.cc

libjvlc_la_CPPFLAGS = `$(VLC_CONFIG) --cflags pic` $(JINCLUDES)
libjvlc_la_LIBADD = ../../../src/libvlc-control.la $(LIBJINCLUDES)

if BUILD_JAVA

lib_LTLIBRARIES = libjvlc.la

if HAVE_WIN32
DATA_noinst_libjvlc = jvlc$(LIBEXT) libjvlc$(LIBEXT).a
LIBRARIES_libvlc = $(top_builddir)/src/.libs/libvlc.dll.a $(abs_top_srcdir)/src/.libs/libvlc-control.dll.a
LDFLAGS_jvlc = `$(VLC_CONFIG) --libs libvlc`
libjvlc_DATA = libjvlc.def libjvlc.dll.a jvlc$(LIBEXT) 

libjvlcdir=$(srcdir)

libjvlc.dll.a libjvlc.def:
	$(DLLTOOL) -z libjvlc.def -l libjvlc.dll.a -D jvlc.dll $< $(libjvlc_a_OBJECTS)

jvlc$(LIBEXT): $(libjvlc_a_OBJECTS) \
                      $(libjvlc_a_DEPENDENCIES)
		  $(CXXLINK) $(libjvlc_a_OBJECTS) -Wl,--kill-at \
		        $(srcdir)/libjvlc.def \
		        $(LIBRARIES_libvlc) -shared $(LIBRARIES_libvlc) $(LIBJINCLUDES) \
				$(LDFLAGS_jvlc) $(LTLIBINTL)
# Cygwin work-around, copied by other makefiles
			@if test -f "$@.exe"; then mv -f "$@.exe" "$@"; fi


all: libjvlc.dll.a

endif

.PHONY: clean-local

clean-local:
	rm -f *.o *~ *.def *.dll

endif
