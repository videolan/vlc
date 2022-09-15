# GPGERROR
GPGERROR_VERSION := 1.27
GPGERROR_URL := https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download_pkg,$(GPGERROR_URL),gpg-error)

ifeq ($(call need_pkg,"gpg-error >= 1.27"),)
PKGS_FOUND += gpg-error
endif

.sum-gpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2 .sum-gpg-error
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gpg-error/windres-make.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/gpg-error/winrt.patch
endif
endif
	$(APPLY) $(SRC)/gpg-error/missing-unistd-include.patch
	$(APPLY) $(SRC)/gpg-error/no-executable.patch
	$(APPLY) $(SRC)/gpg-error/win32-unicode.patch
	$(APPLY) $(SRC)/gpg-error/version-bump-gawk-5.patch
	$(APPLY) $(SRC)/gpg-error/win32-extern-struct.patch
	$(APPLY) $(SRC)/gpg-error/darwin-triplet.patch
ifndef HAVE_WIN32
	cp -f -- "$(SRC)/gpg-error/lock-obj-pub.posix.h" \
		"$(UNPACK_DIR)/src/lock-obj-pub.native.h"
endif
	$(MOVE)

GPGERROR_CONF := $(HOSTCONF) \
	--disable-nls \
	--disable-languages \
	--disable-tests

.gpg-error: libgpg-error
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(GPGERROR_CONF)
	# pre_mkheader_cmds would delete our lock-obj-pub-native.h
	cd $< && $(MAKE) pre_mkheader_cmds=true install
	touch $@
