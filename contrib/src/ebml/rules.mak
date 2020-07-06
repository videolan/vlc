# ebml

EBML_VERSION := 1.4.0
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.xz

ifeq ($(call need_pkg,"libebml >= 1.3.8"),)
PKGS_FOUND += ebml
endif

$(TARBALLS)/libebml-$(EBML_VERSION).tar.xz:
	$(call download_pkg,$(EBML_URL),ebml)

.sum-ebml: libebml-$(EBML_VERSION).tar.xz

ebml: libebml-$(EBML_VERSION).tar.xz .sum-ebml
	$(UNPACK)
	$(MOVE)

.ebml: ebml toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DENABLE_WIN32_IO=OFF
	cd $< && $(MAKE) install
	touch $@
