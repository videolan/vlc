# GPGERROR
GPGERROR_VERSION := 1.20
GPGERROR_URL := ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download_pkg,$(GPGERROR_URL),gpg-error)

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
	$(MOVE)
	cp $@/src/syscfg/lock-obj-pub.arm-unknown-linux-androideabi.h $@/src/syscfg/lock-obj-pub.linux-android.h
ifdef HAVE_TIZEN
ifeq ($(TIZEN_ABI), x86)
	cp $@/src/syscfg/lock-obj-pub.i686-pc-linux-gnu.h $@/src/syscfg/lock-obj-pub.linux-gnueabi.h
endif
endif
ifdef HAVE_IOS
ifdef HAVE_ARMV7A
	cp $@/src/syscfg/lock-obj-pub.arm-apple-darwin.h $@/src/syscfg/lock-obj-pub.$(HOST).h
else
	cp $@/src/syscfg/lock-obj-pub.aarch64-apple-darwin.h $@/src/syscfg/lock-obj-pub.$(HOST).h
endif
endif

.gpg-error: libgpg-error
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-nls --disable-shared --disable-languages
	cd $< && $(MAKE) install
	touch $@
