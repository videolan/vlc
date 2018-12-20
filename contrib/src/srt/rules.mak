# srt

SRT_VERSION := 1.2.3
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += srt
endif

ifeq ($(call need_pkg,"srt >= 1.2.2"),)
PKGS_FOUND += srt
endif

ifdef HAVE_WIN32
DEPS_srt += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/add-implicit-link-libraries.patch
	$(APPLY) $(SRC)/srt/0001-CMakeLists.txt-substitute-link-flags-for-package-nam.patch
	$(APPLY) $(SRC)/srt/0002-CMakeLists.txt-let-cmake-find-pthread.patch
	$(APPLY) $(SRC)/srt/fix-partial-availability.patch
	$(call pkg_static,"scripts/haisrt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

DEPS_srt = gnutls $(DEPS_gnutls)

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON -DENABLE_CXX11=OFF
	cd $< && $(MAKE) install
	touch $@
