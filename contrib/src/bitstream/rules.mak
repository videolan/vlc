BITSTREAM_HASH := a9b69ce2680ee361e0327ce0c8c2fdee48e390c3
BITSTREAM_VERSION := git-$(BITSTREAM_HASH)
BITSTREAM_GITURL := https://code.videolan.org/videolan/bitstream.git

PKGS += bitstream

$(TARBALLS)/bitstream-$(BITSTREAM_VERSION).tar.xz:
	$(call download_git,$(BITSTREAM_GITURL),,$(BITSTREAM_HASH))

.sum-bitstream: bitstream-$(BITSTREAM_VERSION).tar.xz
	$(call check_githash,$(BITSTREAM_HASH))
	touch $@

bitstream: bitstream-$(BITSTREAM_VERSION).tar.xz .sum-bitstream
	$(UNPACK)
	$(MOVE)

.bitstream: bitstream
	cd $< && PREFIX=$(PREFIX) $(MAKE) install
	touch $@
