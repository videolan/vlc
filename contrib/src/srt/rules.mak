# srt

SRT_VERSION := 1.2.2
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifndef HAVE_WIN32
ifdef BUILD_NETWORK
PKGS += srt
endif
endif
ifeq ($(call need_pkg,"srt >= 1.2.2"),)
PKGS_FOUND += srt
endif

ifdef HAVE_DARWIN_OS
SRT_DARWIN=CFLAGS="$(CFLAGS) -Wno-error=partial-availability" CXXFLAGS="$(CXXFLAGS) -Wno-error=partial-availability"
endif

$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/fix-pc.patch
	$(APPLY) $(SRC)/srt/add-implicit-link-libraries.patch
	$(call pkg_static,"scripts/haisrt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

DEPS_srt = gnutls $(DEPS_gnutls)

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(SRT_DARWIN) $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON -DENABLE_CXX11=OFF
	cd $< && $(MAKE) install
	touch $@
