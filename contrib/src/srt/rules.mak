# srt

SRT_VERSION := 1.2.3
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += srt
endif

ifeq ($(call need_pkg,"srt >= 1.2.2"),)
PKGS_FOUND += srt
endif

SRT_CFLAGS   := $(CFLAGS)
SRT_CXXFLAGS := $(CXXFLAGS)
DEPS_srt = gnutls $(DEPS_gnutls)
ifdef HAVE_WIN32
DEPS_srt += pthreads $(DEPS_pthreads)
endif

ifdef HAVE_DARWIN_OS
SRT_CFLAGS   += -Wno-error=partial-availability
SRT_CXXFLAGS += -Wno-error=partial-availability
endif

$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/add-implicit-link-libraries.patch 
	$(APPLY) $(SRC)/srt/0001-CMakeLists.txt-substitute-link-flags-for-package-nam.patch
	$(APPLY) $(SRC)/srt/srt-fix-non-gnu-detection.patch 
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/srt/0002-CMakeLists.txt-let-cmake-find-pthread.patch
	$(APPLY) $(SRC)/srt/srt-no-implicit-libs.patch 
endif
	$(APPLY) $(SRC)/srt/srt-fix-compiler-detect.patch
	$(call pkg_static,"scripts/haisrt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) CFLAGS="$(SRT_CFLAGS)" CXXFLAGS="$(SRT_CXXFLAGS)" $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON -DENABLE_CXX11=OFF
	cd $< && $(MAKE) install
	touch $@
