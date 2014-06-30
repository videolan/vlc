# chromaprint

CHROMAPRINT_VERSION := 1.0
CHROMAPRINT_URL := http://bitbucket.org/acoustid/chromaprint/downloads/chromaprint-$(CHROMAPRINT_VERSION).tar.gz

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
	$(APPLY) $(SRC)/chromaprint/mavericks.patch
	$(APPLY) $(SRC)/chromaprint/osxpkgconfig.patch
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

.chromaprint: chromaprint .ffmpeg toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DBUILD_SHARED_LIBS:BOOL=OFF
	cd $< && $(MAKE) install
	touch $@
