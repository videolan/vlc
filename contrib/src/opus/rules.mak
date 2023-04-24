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
	$(APPLY) $(SRC)/opus/0001-meson-arm64.patch
	$(MOVE)

OPUS_CONF=  -D extra-programs=disabled -D tests=disabled -D docs=disabled
ifndef HAVE_FPU
OPUS_CONF += -D fixed-point=true
endif

.opus: opus crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) $(OPUS_CONF)
	+$(MESONBUILD)
	touch $@
