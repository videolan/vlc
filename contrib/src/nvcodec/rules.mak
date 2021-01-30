NVCODEC_HASH := b6600f507de70d223101fe98f9c3c351b724e2fa
NVCODEC_GITURL := https://git.videolan.org/git/ffmpeg/nv-codec-headers.git

ifndef HAVE_DARWIN_OS
PKGS += nvcodec
endif

$(TARBALLS)/nvcodec-$(NVCODEC_HASH).tar.xz:
	$(call download_git,$(NVCODEC_GITURL),,$(NVCODEC_HASH))

.sum-nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz

nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz .sum-nvcodec
	$(UNPACK)
	$(MOVE)

.nvcodec: nvcodec-$(NVCODEC_HASH).tar.xz nvcodec
	cd nvcodec && make install PREFIX="$(PREFIX)"
	touch $@
