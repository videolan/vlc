# ebml

EBML_VERSION := 1.3.5
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.xz

ifeq ($(call need_pkg,"libebml"),)
PKGS_FOUND += ebml
endif

$(TARBALLS)/libebml-$(EBML_VERSION).tar.xz:
	$(call download_pkg,$(EBML_URL),ebml)

.sum-ebml: libebml-$(EBML_VERSION).tar.xz

ebml: libebml-$(EBML_VERSION).tar.xz .sum-ebml
	$(UNPACK)
	$(APPLY) $(SRC)/ebml/ebml-maxread.patch
	$(APPLY) $(SRC)/ebml/unknown-check.patch
	$(APPLY) $(SRC)/ebml/max-size-loop.patch
	$(APPLY) $(SRC)/ebml/ebml-end-boundary.patch
	$(APPLY) $(SRC)/ebml/ebml-null-compare.patch
	$(APPLY) $(SRC)/ebml/ebml-infinite-start.patch
	$(MOVE)

# libebml requires exceptions
EBML_EXTRA_FLAGS = CXXFLAGS="${CXXFLAGS} -fexceptions -fvisibility=hidden"
ifdef HAVE_ANDROID
EBML_EXTRA_FLAGS += CPPFLAGS=""
endif

.ebml: ebml
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(EBML_EXTRA_FLAGS)
	cd $< && $(MAKE) install
	touch $@
