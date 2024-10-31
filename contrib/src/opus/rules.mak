# opus

OPUS_VERSION := 1.5.2

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
	$(MOVE)

OPUS_CONF=  -D extra-programs=disabled -D tests=disabled -D docs=disabled
ifndef HAVE_FPU
OPUS_CONF += -D fixed-point=true
endif

# disable rtcd on aarch64-windows
ifeq ($(ARCH)-$(HAVE_WIN32),aarch64-1)
OPUS_CONF += -D rtcd=disabled
endif
# disable rtcd on armv7-windows
ifeq ($(ARCH)-$(HAVE_WIN32),arm-1)
OPUS_CONF += -D rtcd=disabled
endif

.opus: opus crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(OPUS_CONF)
	+$(MESONBUILD)
	touch $@
