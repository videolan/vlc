# FLAC

FLAC_VERSION := 1.4.2
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
	$(APPLY) $(SRC)/flac/0001-Fixed-compilation-of-get_utf8_argv-for-Windows-UWP.patch
	# use fseek/ftell on 32-bit Android < 24
	$(APPLY) $(SRC)/flac/0001-include-share-compat.h-use-fseek-if-fseeko-is-not-av.patch
	$(APPLY) $(SRC)/flac/0002-CMake-disable-fseeko-on-32-bit-Android-before-API-24.patch
	# disable building a tool we don't use
	sed -e 's,add_subdirectory("microbench"),#add_subdirectory("microbench"),' -i.orig $(UNPACK_DIR)/CMakeLists.txt
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
