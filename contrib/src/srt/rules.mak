# srt

SRT_VERSION := 1.2.2
SRT_TARBALL := v$(SRT_VERSION).tar.gz
SRT_URL := $(GITHUB)/Haivision/srt/archive/$(SRT_TARBALL)

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

DEPS_srt = $(DEPS_gnutls)

.srt: srt
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-gnutls
	cd $< && $(MAKE) install
	touch $@
