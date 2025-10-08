# SMB2
SMB2_VERSION := 6.1
SMB2_URL := $(GITHUB)/sahlberg/libsmb2/archive/refs/tags/libsmb2-$(SMB2_VERSION).tar.gz

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

smb2: UNPACK_DIR=libsmb2-libsmb2-$(SMB2_VERSION)
smb2: libsmb2-$(SMB2_VERSION).tar.gz .sum-smb2
	$(UNPACK)
	$(APPLY) $(SRC)/smb2/0001-cmake-add-ENABLE_LIBKRB5-and-ENABLE_GSSAPI-options.patch
	$(APPLY) $(SRC)/smb2/0001-fix-Fixed-undeclared-identifier-ENXIO-in-android.patch
	$(MOVE)

SMB2_CONF := -DENABLE_LIBKRB5=OFF

.smb2: smb2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SMB2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
