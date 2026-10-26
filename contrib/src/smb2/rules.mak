# SMB2
SMB2_VERSION := 6.3.1
SMB2_URL := $(GITHUB)/sahlberg/libsmb2/archive/refs/tags/libsmb2-$(SMB2_VERSION).tar.gz

ifeq ($(call need_pkg,"smb2"),)
PKGS_FOUND += smb2
endif

$(TARBALLS)/libsmb2-$(SMB2_VERSION).tar.gz:
	$(call download_pkg,$(SMB2_URL),smb2)

.sum-smb2: libsmb2-$(SMB2_VERSION).tar.gz

smb2: UNPACK_DIR=libsmb2-libsmb2-$(SMB2_VERSION)
smb2: libsmb2-$(SMB2_VERSION).tar.gz .sum-smb2
	$(UNPACK)
	$(MOVE)

SMB2_CONF := -DENABLE_LIBKRB5=OFF

.smb2: smb2
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SMB2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
