# SMB2
SMB2_VERSION := 4.0.0
SMB2_URL := $(GITHUB)/sahlberg/libsmb2/archive/v$(SMB2_VERSION).tar.gz

ifdef BUILD_NETWORK
ifndef HAVE_WIN32
PKGS += smb2
endif
endif
ifeq ($(call need_pkg,"smb2"),)
PKGS_FOUND += smb2
endif

$(TARBALLS)/libsmb2-$(SMB2_VERSION).tar.gz:
	$(call download_pkg,$(SMB2_URL),smb2)

.sum-smb2: libsmb2-$(SMB2_VERSION).tar.gz

smb2: libsmb2-$(SMB2_VERSION).tar.gz .sum-smb2
	$(UNPACK)
	$(MOVE)

SMB2_CONF := --disable-examples --disable-werror --without-libkrb5

.smb2: smb2
	cd $< && ./bootstrap
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SMB2_CONF)
	$(MAKEBUILD) install
	touch $@
