# srt

SRT_VERSION := 1.2.2
SRT_TARBALL := srt-v$(SRT_VERSION).tar.gz
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifndef HAVE_WIN32
ifdef BUILD_NETWORK
PKGS += srt
endif
endif
ifeq ($(call need_pkg,"srt >= 1.2.2"),)
PKGS_FOUND += srt
endif

$(TARBALLS)/$(SRT_TARBALL):
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: $(SRT_TARBALL)

srt: $(SRT_TARBALL) .sum-srt
	$(UNPACK)
	mv srt-$(SRT_VERSION) $@ && touch $@

DEPS_srt = gnutls $(DEPS_gnutls)

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON
	cd $< && $(MAKE) install
	touch $@
