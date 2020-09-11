NVCODEC_HASH := 84483da70d903239d4536763fde8c7e6c4e80784
NVCODEC_GITURL := https://git.videolan.org/git/ffmpeg/nv-codec-headers.git

ifndef HAVE_DARWIN_OS
PKGS += nvcodec
endif

$(TARBALLS)/nvcodec-$(NVCODEC_HASH).tar.xz:
	$(call download_git,$(NVCODEC_GITURL),,$(NVCODEC_HASH))

.sum-nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz
	$(call check_githash,$(NVCODEC_HASH))
	touch $@

nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz .sum-nvcodec
	$(UNPACK)
	$(MOVE)

.nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz nvcodec
	cd nvcodec && $(MAKE) install PREFIX="$(PREFIX)"
	touch $@
