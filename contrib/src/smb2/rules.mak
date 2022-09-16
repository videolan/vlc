# SMB2
SMB2_VERSION := 4.0.0
SMB2_URL := $(GITHUB)/sahlberg/libsmb2/archive/v$(SMB2_VERSION).tar.gz

ifeq ($(call need_pkg,"smb2"),)
PKGS_FOUND += smb2
endif

$(TARBALLS)/libsmb2-$(SMB2_VERSION).tar.gz:
	$(call download_pkg,$(SMB2_URL),smb2)

.sum-smb2: libsmb2-$(SMB2_VERSION).tar.gz

smb2: libsmb2-$(SMB2_VERSION).tar.gz .sum-smb2
	$(UNPACK)
	$(MOVE)

.smb2: smb2
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-werror --without-libkrb5 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
