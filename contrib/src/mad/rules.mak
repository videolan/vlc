# mad

MAD_VERSION := 0.16.4
MAD_URL := https://codeberg.org/tenacityteam/libmad/archive/$(MAD_VERSION).tar.gz

ifdef GPL
PKGS += mad
endif
ifeq ($(call need_pkg,"mad"),)
PKGS_FOUND += mad
endif

MAD_CONF := -DEXAMPLE=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
MAD_CONF += -DASO:BOOL=OFF
endif
endif

$(TARBALLS)/libmad-$(MAD_VERSION).tar.gz:
	$(call download,$(MAD_URL))

.sum-mad: libmad-$(MAD_VERSION).tar.gz

libmad: UNPACK_DIR=libmad
libmad: libmad-$(MAD_VERSION).tar.gz .sum-mad
	$(UNPACK)
ifdef HAVE_IOS
	$(APPLY) $(SRC)/mad/mad-ios-asm.patch
endif
	# old patch seem to solve buffer overflow a different way
	# $(APPLY) $(SRC)/mad/check-bitstream-length.patch
	# get a tarball with a folder name $(MOVE)

.mad: libmad toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(MAD_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
