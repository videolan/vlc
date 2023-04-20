# opus

OPUS_VERSION := 1.4

OPUS_URL := $(XIPH)/opus/opus-$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download_pkg,$(OPUS_URL),opus)

.sum-opus: opus-$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(APPLY) $(SRC)/opus/0002-CMake-set-the-pkg-config-string-as-with-autoconf-mes.patch
	# fix missing included file in packaged source
	cd $(UNPACK_DIR) && sed -e 's,include(opus_buildtype,#include(opus_buildtype,' -i.orig CMakeLists.txt
	$(MOVE)

OPUS_CONF=
ifndef HAVE_FPU
OPUS_CONF += -DOPUS_FIXED_POINT=ON
endif

# rtcd is not working on win64-arm64
ifdef HAVE_WIN64
ifeq ($(ARCH),aarch64)
OPUS_CONF += -DOPUS_MAY_HAVE_NEON=OFF -DOPUS_PRESUME_NEON=ON
endif
endif



.opus: opus toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(OPUS_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
