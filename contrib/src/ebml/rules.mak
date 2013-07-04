# ebml

EBML_VERSION := 1.3.0
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.bz2
#EBML_URL := $(CONTRIB_VIDEOLAN)/libebml-$(EBML_VERSION).tar.bz2

$(TARBALLS)/libebml-$(EBML_VERSION).tar.bz2:
	$(call download,$(EBML_URL))

.sum-ebml: libebml-$(EBML_VERSION).tar.bz2

libebml: libebml-$(EBML_VERSION).tar.bz2 .sum-ebml
	$(UNPACK)
	$(APPLY) $(SRC)/ebml/ebml-pic.patch
	$(APPLY) $(SRC)/ebml/no-ansi.patch
	$(MOVE)

# libebml requires exceptions
EBML_EXTRA_FLAGS = CXXFLAGS="${CXXFLAGS} -fexceptions" \
					CPPFLAGS=""

.ebml: libebml
ifdef HAVE_WIN32
	cd $< && $(MAKE) -C make/mingw32 prefix="$(PREFIX)" $(HOSTVARS) SHARED=no
else
	cd $< && $(MAKE) -C make/linux prefix="$(PREFIX)" $(HOSTVARS) $(EBML_EXTRA_FLAGS) staticlib
endif
	cd $< && $(MAKE) -C make/linux install_staticlib install_headers prefix="$(PREFIX)" $(HOSTVARS)
	$(RANLIB) "$(PREFIX)/lib/libebml.a"
	touch $@
