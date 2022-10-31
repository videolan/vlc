# speexdsp

SPEEXDSP_VERSION := 1.2.1
SPEEXDSP_URL := $(XIPH)/speex/speexdsp-$(SPEEXDSP_VERSION).tar.gz

PKGS += speexdsp
ifeq ($(call need_pkg,"speexdsp"),)
PKGS_FOUND += speexdsp
endif

$(TARBALLS)/speexdsp-$(SPEEXDSP_VERSION).tar.gz:
	$(call download_pkg,$(SPEEXDSP_URL),speexdsp)

.sum-speexdsp: speexdsp-$(SPEEXDSP_VERSION).tar.gz

speexdsp: speexdsp-$(SPEEXDSP_VERSION).tar.gz .sum-speexdsp
	$(UNPACK)
	$(call pkg_static,"speexdsp.pc.in")
	$(APPLY) $(SRC)/speexdsp/missing-stdint-for-aarch.patch
	$(MOVE)

SPEEXDSP_CONF := --enable-resample-full-sinc-table --disable-examples
ifeq ($(filter arm aarch64, $(ARCH)),)
# The configure script checks for NEON C intrinsics only.
# This leads to false positives on Android-x86.
SPEEXDSP_CONF += --disable-neon
endif
ifndef HAVE_FPU
SPEEXDSP_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEXDSP_CONF += --enable-arm5e-asm
endif
endif

.speexdsp: speexdsp
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SPEEXDSP_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
