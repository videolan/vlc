# ebml

EBML_VERSION := 1.4.2
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.xz

ifeq ($(call need_pkg,"libebml >= 1.3.8"),)
PKGS_FOUND += ebml
endif

$(TARBALLS)/libebml-$(EBML_VERSION).tar.xz:
	$(call download_pkg,$(EBML_URL),ebml)

.sum-ebml: libebml-$(EBML_VERSION).tar.xz

ebml: libebml-$(EBML_VERSION).tar.xz .sum-ebml
	$(UNPACK)
	$(APPLY) $(SRC)/ebml/missing-limits-include.patch
	$(MOVE)

.ebml: ebml toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE)
	+$(CMAKEBUILD) $< --target install
	touch $@
