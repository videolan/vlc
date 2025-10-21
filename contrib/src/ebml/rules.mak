# ebml

EBML_VERSION := 1.4.3
EBML_URL := https://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.xz

ifeq ($(call need_pkg,"libebml >= 1.3.8"),)
PKGS_FOUND += ebml
endif

$(TARBALLS)/libebml-$(EBML_VERSION).tar.xz:
	$(call download_pkg,$(EBML_URL),ebml)

.sum-ebml: libebml-$(EBML_VERSION).tar.xz

ebml: libebml-$(EBML_VERSION).tar.xz .sum-ebml
	$(UNPACK)
	$(APPLY) $(SRC)/ebml/0001-EbmlString-ReadFully-use-automatic-memory-management.patch
	$(APPLY) $(SRC)/ebml/0002-EbmlUnicodeString-use-std-string-when-reading-instea.patch
	$(APPLY) $(SRC)/ebml/0001-EbmlMaster-fix-leak-when-reading-upper-level-element.patch
	$(MOVE)

.ebml: ebml toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
