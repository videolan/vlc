###############################################################################
# Building the Activex plugin
###############################################################################

MOSTLYCLEANFILES = 
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
    position.h \
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
    guiddef.h \
    $(NULL)

DIST_rsrc = \
    axvlc_rc.rc.in \
    $(NULL)

DIST_misc = \
    README.TXT \
    axvlc.inf.in \
    axvlc.def \
    axvlc.dll.manifest \
    axvlc.idl \
    axvlc.tlb \
    inplace.bmp \
    vlc16x16.bmp \
    test.html \
    $(NULL)

if BUILD_ACTIVEX

lib_LTLIBRARIES = axvlc.la

LIBRARIES_libvlc = $(top_builddir)/src/libvlc.la \
				   $(top_builddir)/src/libvlccore.la

axvlc_la_SOURCES = $(SOURCES_activex)
axvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags activex`
axvlc_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags activex`
axvlc_la_DEPENDENCIES = axvlc.def $(DATA_axvlc_rc) $(LIBRARIES_libvlc)
axvlc_la_LDFLAGS = -Wl,$(srcdir)/axvlc.def -Wl,$(DATA_axvlc_rc) \
    -no-undefined -avoid-version -module \
    `$(VLC_CONFIG) --ldflags activex libvlc`
axvlc_la_LIBADD = $(LIBRARIES_libvlc) \
	`$(VLC_CONFIG) -libs activex`

DATA_axvlc_rc = $(noinst_axvlc_rc_DATA)
noinst_axvlc_rc_DATA = axvlc_rc.$(OBJEXT)
noinst_axvlc_rcdir = 
axvlc_rc.$(OBJEXT): axvlc_rc.rc inplace.bmp axvlc.tlb
	$(WINDRES) --include-dir $(srcdir) -i $< -o $@

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

else

clean-tlb:

endif

###############################################################################
# Clean rules
###############################################################################

clean-local: clean-tlb

###############################################################################
# Force rules
###############################################################################

