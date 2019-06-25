NVCODEC_HASH := 9fdaf11b8f79d4e41cde9af89656238f25fec6fd
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
