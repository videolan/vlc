# srt

SRT_VERSION := 1.2.2
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += srt
endif

ifeq ($(call need_pkg,"srt >= 1.2.2"),)
PKGS_FOUND += srt
endif

ifdef HAVE_DARWIN_OS
SRT_DARWIN=CFLAGS="$(CFLAGS) -Wno-error=partial-availability" CXXFLAGS="$(CXXFLAGS) -Wno-error=partial-availability"
endif

ifdef HAVE_WIN32
DEPS_srt += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/fix-pc.patch
	$(APPLY) $(SRC)/srt/add-implicit-link-libraries.patch
	$(APPLY) $(SRC)/srt/0001-srtcore-api.h-change-inet_ntop-to-getnameinfo.patch
	$(APPLY) $(SRC)/srt/0001-Clean-.pc-to-provide-the-dependecies.patch
	$(APPLY) $(SRC)/srt/0001-srt_compat.h-Enable-localtime_s-only-if-MSC_VER-1500.patch
	$(APPLY) $(SRC)/srt/0001-channel.cpp-add-mswsock.h-for-Win32.patch
	$(APPLY) $(SRC)/srt/0001-Fix-include-path-for-wintime.h.patch
	$(APPLY) $(SRC)/srt/0001-CMakeLists.txt-let-cmake-find-pthread.patch
	$(call pkg_static,"scripts/haisrt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

DEPS_srt = gnutls $(DEPS_gnutls)

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(SRT_DARWIN) $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON -DENABLE_CXX11=OFF
	cd $< && $(MAKE) install
	touch $@
