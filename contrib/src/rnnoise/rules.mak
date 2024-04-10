# rnnoise

RNNOISE_VERSION := 0.1.1
RNNOISE_URL := $(GITHUB)/xiph/rnnoise/archive/refs/tags/v${RNNOISE_VERSION}.tar.gz

PKGS += rnnoise

ifeq ($(call need_pkg,"rnnoise"),)
PKGS_FOUND += rnnoise
endif

$(TARBALLS)/rnnoise-$(RNNOISE_VERSION).tar.gz:
	$(call download_pkg,$(RNNOISE_URL),rnnoise)

.sum-rnnoise: rnnoise-$(RNNOISE_VERSION).tar.gz

rnnoise: rnnoise-$(RNNOISE_VERSION).tar.gz .sum-rnnoise
	$(UNPACK)
	$(MOVE)

RNNOISE_CONF := --disable-examples --disable-doc

.rnnoise: rnnoise
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(RNNOISE_CONF)
	+$(MAKEBUILD)
	$(call pkg_static,"$(BUILD_DIRUNPACK)/rnnoise.pc")
	+$(MAKEBUILD) install
	touch $@
