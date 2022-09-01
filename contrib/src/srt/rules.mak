# srt

SRT_VERSION := 1.4.4
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += srt
endif

ifeq ($(call need_pkg,"srt >= 1.3.2"),)
PKGS_FOUND += srt
endif

DEPS_srt = gnutls $(DEPS_gnutls)
ifdef HAVE_WIN32
DEPS_srt += pthreads $(DEPS_pthreads)
endif


$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/0001-core-remove-MSG_TRUNC-logging.patch
	$(APPLY) $(SRC)/srt/0001-build-always-use-GNUInstallDirs.patch
	$(call pkg_static,"scripts/srt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

SRT_CONF := -DENABLE_SHARED=OFF -DUSE_ENCLIB=gnutls -DENABLE_CXX11=OFF

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) $(SRT_CONF)
	+$(CMAKEBUILD) $< --target install
	touch $@
