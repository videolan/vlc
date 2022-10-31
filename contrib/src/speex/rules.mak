# speex

SPEEX_VERSION := 1.2.1
SPEEX_URL := http://downloads.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz

PKGS += speex
ifeq ($(call need_pkg,"speex >= 1.0.5"),)
PKGS_FOUND += speex
endif

$(TARBALLS)/speex-$(SPEEX_VERSION).tar.gz:
	$(call download_pkg,$(SPEEX_URL),speex)

.sum-speex: speex-$(SPEEX_VERSION).tar.gz

speex: speex-$(SPEEX_VERSION).tar.gz .sum-speex
	$(UNPACK)
	$(call pkg_static,"speex.pc.in")
	$(MOVE)

SPEEX_CONF := --disable-binaries
ifndef HAVE_FPU
SPEEX_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEX_CONF += --enable-arm5e-asm
endif
endif
ifeq ($(ARCH),aarch64)
SPEEX_CONF += --disable-neon
endif

.speex: speex
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SPEEX_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
