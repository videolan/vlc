# FLAC

FLAC_VERSION := 1.5.0
FLAC_URL := $(GITHUB)/xiph/flac/releases/download/$(FLAC_VERSION)/flac-$(FLAC_VERSION).tar.xz

PKGS += flac
ifeq ($(call need_pkg,"flac"),)
PKGS_FOUND += flac
endif

$(TARBALLS)/flac-$(FLAC_VERSION).tar.xz:
	$(call download_pkg,$(FLAC_URL),flac)

.sum-flac: flac-$(FLAC_VERSION).tar.xz

flac: flac-$(FLAC_VERSION).tar.xz .sum-flac
	$(UNPACK)
	$(APPLY) $(SRC)/flac/0001-cmake-Include-pthread-in-the-pkg-config-file-if-usin.patch
	$(call pkg_static,"src/libFLAC/flac.pc.in")
	$(MOVE)

FLAC_CONF = \
	-DINSTALL_MANPAGES=OFF \
	-DBUILD_CXXLIBS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_PROGRAMS=OFF \
	-DBUILD_DOCS=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Iconv=ON

ifeq ($(ARCH),i386)
# nasm doesn't like the -fstack-protector-strong that's added to its flags
# let's prioritize the use of nasm over stack protection
FLAC_CONF += -DWITH_STACK_PROTECTOR=OFF
endif

DEPS_flac = ogg $(DEPS_ogg)

.flac: flac toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(FLAC_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
