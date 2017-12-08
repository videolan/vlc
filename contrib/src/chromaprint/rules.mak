# chromaprint

CHROMAPRINT_VERSION := 1.4.2
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
	$(APPLY) $(SRC)/chromaprint/linklibs.patch
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

.chromaprint: chromaprint .ffmpeg toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DBUILD_SHARED_LIBS:BOOL=OFF
	cd $< && $(MAKE) install
	touch $@
