# FLAC

FLAC_VERSION := 1.4.0
FLAC_URL := http://downloads.xiph.org/releases/flac/flac-$(FLAC_VERSION).tar.xz

PKGS += flac
ifeq ($(call need_pkg,"flac"),)
PKGS_FOUND += flac
endif

$(TARBALLS)/flac-$(FLAC_VERSION).tar.xz:
	$(call download_pkg,$(FLAC_URL),flac)

.sum-flac: flac-$(FLAC_VERSION).tar.xz

flac: flac-$(FLAC_VERSION).tar.xz .sum-flac
	$(UNPACK)
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/flac/console_write.patch
	$(APPLY) $(SRC)/flac/remove_blocking_code_useless_flaclib.patch
	$(APPLY) $(SRC)/flac/no-createfilew.patch
endif
	# disable building a tool we don't use
	cd $(UNPACK_DIR) && sed -e 's,add_subdirectory("microbench"),#add_subdirectory("microbench"),' -i.orig CMakeLists.txt
	$(call pkg_static,"src/libFLAC/flac.pc.in")
	$(MOVE)

FLAC_CONF = \
	-DBUILD_TESTING=OFF \
	-DINSTALL_MANPAGES=OFF \
	-DBUILD_CXXLIBS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_PROGRAMS=OFF \
	-DBUILD_DOCS=OFF

ifeq ($(ARCH),i386)
# nasm doesn't like the -fstack-protector-strong that's added to its flags
# let's prioritize the use of nasm over stack protection
FLAC_CONF += -DWITH_STACK_PROTECTOR=OFF
endif

DEPS_flac = ogg $(DEPS_ogg)

.flac: flac toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_PIC) $(CMAKE) $(FLAC_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
