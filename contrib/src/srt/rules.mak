# srt

SRT_VERSION := 1.5.4
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

# gnutls (nettle/gmp) can't be used with the LGPLv2 license
ifdef GPL
SRT_PKG=1
else
ifdef GNUV3
SRT_PKG=1
endif
endif

ifdef BUILD_NETWORK
ifdef SRT_PKG
PKGS += srt
endif
endif

ifeq ($(call need_pkg,"srt >= 1.3.2"),)
PKGS_FOUND += srt
endif

DEPS_srt = gnutls $(DEPS_gnutls)
ifdef HAVE_WIN32
DEPS_srt += winpthreads $(DEPS_winpthreads)
endif


$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/0001-build-fix-implicit-libraries-set-using-Wl-l-libname..patch
	$(call pkg_static,"scripts/srt.pc.in")
	$(MOVE)

SRT_CONF := -DENABLE_SHARED=OFF -DUSE_ENCLIB=gnutls -DENABLE_CXX11=OFF -DENABLE_APPS=OFF

.srt: srt toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SRT_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
