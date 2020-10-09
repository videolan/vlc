# SMB2
SMB2_VERSION := 4a842524407b0c0b87b1f62fe792d569d92cd34d
SMB2_URL := https://github.com/sahlberg/libsmb2/archive/$(SMB2_VERSION).tar.gz

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

.smb2: smb2
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-werror --without-libkrb5 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
