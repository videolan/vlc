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
	$(MOVE)
	cp $@/src/syscfg/lock-obj-pub.arm-unknown-linux-androideabi.h $@/src/syscfg/lock-obj-pub.linux-android.h
ifdef HAVE_IOS
	cp $@/src/syscfg/lock-obj-pub.arm-unknown-linux-androideabi.h $@/src/syscfg/lock-obj-pub.$(HOST).h
endif

.gpg-error: libgpg-error
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-nls --disable-shared --disable-languages
	cd $< && $(MAKE) install
	touch $@
