# opus

OPUS_VERSION := 1.3.1

OPUS_URL := https://archive.mozilla.org/pub/opus/opus-$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download_pkg,$(OPUS_URL),opus)

.sum-opus: opus-$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(APPLY) $(SRC)/opus/0001-CMake-set-the-pkg-config-version-to-the-library-vers.patch
	$(APPLY) $(SRC)/opus/0002-CMake-set-the-pkg-config-string-as-with-autoconf-mes.patch
	# fix missing included file in packaged source
	cd $(UNPACK_DIR) && sed -e 's,include(opus_buildtype,#include(opus_buildtype,' -i.orig CMakeLists.txt
	$(MOVE)

OPUS_CONF=
ifndef HAVE_FPU
OPUS_CONF += -DOPUS_FIXED_POINT=ON
endif

.opus: opus toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_PIC) $(CMAKE) $(OPUS_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
