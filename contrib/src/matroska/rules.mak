# matroska

MATROSKA_VERSION := 1.4.1
MATROSKA_URL := http://dl.matroska.org/downloads/libmatroska/libmatroska-$(MATROSKA_VERSION).tar.bz2
#MATROSKA_URL := $(CONTRIB_VIDEOLAN)/libmatroska-$(MATROSKA_VERSION).tar.bz2

PKGS += matroska
DEPS_matroska = ebml $(DEPS_ebml)

$(TARBALLS)/libmatroska-$(MATROSKA_VERSION).tar.bz2:
	$(call download,$(MATROSKA_URL))

.sum-matroska: libmatroska-$(MATROSKA_VERSION).tar.bz2

libmatroska: libmatroska-$(MATROSKA_VERSION).tar.bz2 .sum-matroska
	$(UNPACK)
	$(APPLY) $(SRC)/matroska/matroska-pic.patch
	$(MOVE)

.matroska: libmatroska
ifdef HAVE_WIN32
	cd $< && $(MAKE) -C make/mingw32 prefix="$(PREFIX)" $(HOSTVARS) SHARED=no EBML_DLL=no libmatroska.a
else
	cd $< && $(MAKE) -C make/linux prefix="$(PREFIX)" $(HOSTVARS) staticlib
endif
	cd $< && $(MAKE) -C make/linux install_staticlib install_headers prefix="$(PREFIX)" $(HOSTVARS)
	$(RANLIB) "$(PREFIX)/lib/libmatroska.a"
	touch $@
