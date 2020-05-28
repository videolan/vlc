# SMB2
SMB2_VERSION := 3.0.0
SMB2_URL := https://github.com/sahlberg/libsmb2/archive/v$(SMB2_VERSION).tar.gz

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
	$(APPLY) $(SRC)/smb2/0001-ntlmssp-add-support-for-Anonymous-logins.patch
	$(APPLY) $(SRC)/smb2/0002-Fix-indent-and-white-spaces.patch
	$(APPLY) $(SRC)/smb2/0003-Fix-getlogin-usage.patch
	$(APPLY) $(SRC)/smb2/0004-smb2_destroy_context-fix-possible-null-deref.patch
	$(APPLY) $(SRC)/smb2/0005-smb2_connect_share_async-don-t-return-ENOMEM-if-conn.patch
	$(APPLY) $(SRC)/smb2/0006-IPv6-is-available-on-all-platforms.patch
	$(MOVE)

.smb2: smb2
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-werror --without-libkrb5 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
