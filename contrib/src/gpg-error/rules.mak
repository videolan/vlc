# GPGERROR
GPGERROR_VERSION := 1.18
GPGERROR_URL := ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download,$(GPGERROR_URL))

.sum-gpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2 .sum-gpg-error
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gpg-error/windres-make.patch
endif
	$(APPLY) $(SRC)/gpg-error/missing-unistd-include.patch
	$(APPLY) $(SRC)/gpg-error/no-executable.patch
	$(MOVE)
	cp $@/src/syscfg/lock-obj-pub.arm-unknown-linux-androideabi.h $@/src/syscfg/lock-obj-pub.linux-android.h
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
