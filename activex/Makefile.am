###############################################################################
# Building the Activex plugin
###############################################################################

noinst_LIBRARIES = $(noinst_LIBRARIES_activex)

MOSTLYCLEANFILES = $(LIBRARIES_activex)
EXTRA_DIST = $(SOURCES_activex) $(DIST_rsrc) $(DIST_misc)
BUILT_SOURCES = $(BUILT_SOURCES_activex)

SOURCES_activex = \
    main.cpp \
    utils.cpp \
    utils.h \
    olecontrol.cpp \
    olecontrol.h \
    oleinplaceactiveobject.cpp \
    oleinplaceactiveobject.h \
    oleinplaceobject.cpp \
    oleinplaceobject.h \
    oleobject.cpp \
    oleobject.h \
    persistpropbag.cpp \
    persistpropbag.h \
    persiststorage.cpp \
    persiststorage.h \
    persiststreaminit.cpp \
    persiststreaminit.h \
    provideclassinfo.cpp \
    provideclassinfo.h \
    connectioncontainer.cpp \
    connectioncontainer.h \
    objectsafety.cpp \
    objectsafety.h \
    dataobject.cpp \
    dataobject.h \
    viewobject.cpp \
    viewobject.h \
    supporterrorinfo.cpp \
    supporterrorinfo.h \
    vlccontrol.cpp \
    vlccontrol.h \
    vlccontrol2.cpp \
    vlccontrol2.h \
    plugin.cpp \
    plugin.h \
    axvlc_idl.c \
    axvlc_idl.h \
    $(NULL)

DIST_rsrc = \
    axvlc_rc.rc \
    $(NULL)

DIST_misc = \
    README.TXT \
    axvlc.inf.in \
    axvlc.def \
    axvlc.idl \
    axvlc.tlb \
    inplace.bmp \
    vlc16x16.bmp \
    test.html \
    $(NULL)

if BUILD_SHARED
LIBRARIES_libvlc = $(top_builddir)/src/libvlc.dll
else
LIBRARIES_libvlc = $(top_builddir)/src/libvlc.a
endif

if BUILD_ACTIVEX

noinst_LIBRARIES_activex = libaxvlc.a
axvlc_DATA = axvlc$(LIBEXT)
axvlcdir = $(libdir)

LDFLAGS_activex = `$(VLC_CONFIG) --libs activex vlc builtin`

libaxvlc_a_SOURCES = $(SOURCES_activex) $(SOURCE_dll)
libaxvlc_a_CFLAGS = `$(VLC_CONFIG) --cflags activex`
libaxvlc_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags activex`
libaxvlc_a_DEPENDENCIES = axvlc.def $(DATA_axvlc_rc) $(LIBRARIES_libvlc)

axvlc$(LIBEXT): $(libaxvlc_a_OBJECTS) \
	              $(libaxvlc_a_DEPENDENCIES) stamp-builtin
	$(CXXLINK) $(libaxvlc_a_OBJECTS) $(DATA_axvlc_rc) \
	$(srcdir)/axvlc.def \
	  $(LIBRARIES_libvlc) -shared $(LIBRARIES_libvlc) $(LDFLAGS_activex) \
	  $(INCLUDED_LIBINTL)
# Cygwin work-around
	@if test -f "$@.exe"; then mv -f "$@.exe" "$@"; fi


DATA_axvlc_tlb = $(axvlc_tlb_DATA)
noinst_axvlc_tlb_DATA = axvlc.tlb
noinst_axvlc_tlbdir = $(libdir)

if HAS_MIDL_COMPILER

axvlc.tlb axvlc_idl.c axvlc_idl.h: axvlc.idl
	$(MIDL) -Oicf -error all -env win32 -tlb axvlc.tlb -iid axvlc_idl.c -h axvlc_idl.h axvlc.idl

clean-tlb:
	rm -f axvlc.tlb axvlc_idl.c axvlc_idl.h

else
if HAS_WIDL_COMPILER

axvlc.tlb axvlc_idl.c axvlc_idl.h: axvlc.idl
	$(WIDL) -I$(WINE_SDK_PATH)/include -tuh -T axvlc.tlb -U axvlc_idl.c -H axvlc_idl.h axvlc.idl

clean-tlb:
	rm -f axvlc.tlb axvlc_idl.c axvlc_idl.h

else

clean-tlb:

endif
endif

DATA_axvlc_rc = $(noinst_axvlc_rc_DATA)
noinst_axvlc_rc_DATA = axvlc_rc.$(OBJEXT)
noinst_axvlc_rcdir = $(libdir)
axvlc_rc.$(OBJEXT): axvlc_rc.rc inplace.bmp axvlc.tlb
	$(WINDRES) -DVERSION=$(VERSION) -DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/[^0-9]*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` --include-dir $(srcdir) -i $< -o $@

else

clean-tlb:

endif

###############################################################################
# Stamp rules
###############################################################################

clean-stamp:
	rm -f stamp-pic

if BUILD_SHARED
stamp-builtin:
else
stamp-builtin: $(shell ${VLC_CONFIG} --target builtin)
endif
	@touch $@


###############################################################################
# Clean rules
###############################################################################

clean-local: clean-stamp clean-tlb
	rm -f axvlc$(LIBEXT)

###############################################################################
# Force rules
###############################################################################

