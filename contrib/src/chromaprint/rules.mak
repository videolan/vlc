# chromaprint

CHROMAPRINT_VERSION := 1.6.0
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
	$(APPLY) $(SRC)/chromaprint/0001-add-the-C-runtime-to-the-packages-to-link-to.patch
	$(APPLY) $(SRC)/chromaprint/0002-add-required-FFmpeg-libraries-to-the-generated-pkg-c.patch
	$(APPLY) $(SRC)/chromaprint/0003-chromaprint-add-ability-to-link-with-VDSP-Accelerate.patch
	$(call pkg_static,"libchromaprint.pc.cmake")
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

.chromaprint: chromaprint toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DBUILD_TESTS=OFF
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
