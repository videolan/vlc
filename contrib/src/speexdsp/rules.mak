# speexdsp

SPEEXDSP_VERSION := 1.2.1
SPEEXDSP_URL := http://downloads.us.xiph.org/releases/speex/speexdsp-$(SPEEXDSP_VERSION).tar.gz

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
ifeq ($(ARCH),aarch64)
# old neon, not compatible with aarch64
SPEEXDSP_CONF += --disable-neon
endif
ifeq ($(ARCH),arm)
ifndef HAVE_NEON
SPEEXDSP_CONF += --disable-neon
endif
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
