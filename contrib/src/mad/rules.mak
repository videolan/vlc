# mad

MAD_VERSION := 0.16.5
MAD_URL := https://codeberg.org/tenacityteam/libmad/archive/$(MAD_VERSION).tar.gz

ifdef GPL
PKGS += mad
endif
ifeq ($(call need_pkg,"mad"),)
PKGS_FOUND += mad
endif

MAD_CONF := -DEXAMPLE=OFF
ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
MAD_CONF += -DASO:BOOL=OFF
endif
endif

$(TARBALLS)/libmad-$(MAD_VERSION).tar.gz:
	$(call download_pkg,$(MAD_URL),mad)

.sum-mad: libmad-$(MAD_VERSION).tar.gz

libmad: UNPACK_DIR=libmad
libmad: libmad-$(MAD_VERSION).tar.gz .sum-mad
	$(UNPACK)
	$(APPLY) $(SRC)/mad/0001-CMake-case-of-AND-test.patch
ifdef HAVE_IOS
	$(APPLY) $(SRC)/mad/mad-ios-asm.patch
endif
	# get a tarball with a folder name $(MOVE)
	touch $@

.mad: libmad toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(MAD_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
