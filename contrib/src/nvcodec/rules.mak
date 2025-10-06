# NVCODEC_HASH := 84483da70d903239d4536763fde8c7e6c4e80784
# NVCODEC_GITURL := $(VIDEOLAN_GIT)/ffmpeg/nv-codec-headers.git
# NVCODEC_GITURL := $(GITHUB)/FFmpeg/nv-codec-headers.git
NVCODEC_VERSION := 13.0.19.0
NVCODEC_URL := https://code.ffmpeg.org/FFmpeg/nv-codec-headers/releases/download/n$(NVCODEC_VERSION)/nv-codec-headers-$(NVCODEC_VERSION).tar.gz

ifndef HAVE_DARWIN_OS
ifndef HAVE_WINSTORE # no LoadLibrary
PKGS += nvcodec
endif
endif

# $(TARBALLS)/nvcodec-$(NVCODEC_HASH).tar.xz:
# 	$(call download_git,$(NVCODEC_GITURL),,$(NVCODEC_HASH))

$(TARBALLS)/nv-codec-headers-$(NVCODEC_VERSION).tar.gz:
	$(call download_pkg,$(NVCODEC_URL),nvcodec)

.sum-nvcodec: nv-codec-headers-$(NVCODEC_VERSION).tar.gz

nvcodec: nv-codec-headers-$(NVCODEC_VERSION).tar.gz .sum-nvcodec
	$(UNPACK)
	$(MOVE)

.nvcodec: nvcodec
	$(MAKE) -C nvcodec install PREFIX="$(PREFIX)"
	touch $@
