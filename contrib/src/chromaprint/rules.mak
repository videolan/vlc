# chromaprint

CHROMAPRINT_VERSION := 1.5.1
CHROMAPRINT_URL := $(GITHUB)/acoustid/chromaprint/releases/download/v$(CHROMAPRINT_VERSION)/chromaprint-$(CHROMAPRINT_VERSION).tar.gz

PKGS += chromaprint
ifeq ($(call need_pkg,"libchromaprint"),)
PKGS_FOUND += chromaprint
endif

$(TARBALLS)/chromaprint-$(CHROMAPRINT_VERSION).tar.gz:
	$(call download_pkg,$(CHROMAPRINT_URL),chromaprint)

.sum-chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz

chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz .sum-chromaprint
	$(UNPACK)
	$(APPLY) $(SRC)/chromaprint/0001-contribs-chromaprint-more-fixes-for-.pc-file.patch
	$(APPLY) $(SRC)/chromaprint/0002-add-the-C-runtime-to-the-packages-to-link-to.patch
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

.chromaprint: chromaprint toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DBUILD_TESTS=OFF
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
