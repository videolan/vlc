# chromaprint

CHROMAPRINT_VERSION := 0.7
CHROMAPRINT_URL := https://bitbucket.org/acoustid/chromaprint/downloads/chromaprint-$(CHROMAPRINT_VERSION).tar.gz

PKGS += chromaprint
ifeq ($(call need_pkg,"libchromaprint"),)
PKGS_FOUND += chromaprint
endif

$(TARBALLS)/chromaprint-$(CHROMAPRINT_VERSION).tar.gz:
	$(call download,$(CHROMAPRINT_URL))

.sum-chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz

chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz .sum-chromaprint
	$(UNPACK)
	$(APPLY) $(SRC)/chromaprint/avutil.patch
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

.chromaprint: chromaprint .ffmpeg toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DBUILD_SHARED_LIBS:BOOL=OFF
	cd $< && $(MAKE) install
	touch $@
