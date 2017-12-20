# mpg123
MPG123_VERSION := 1.25.7
MPG123_URL := $(SF)/mpg123/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"libmpg123"),)
PKGS_FOUND += mpg123
endif

MPG123CONF = $(HOSTCONF)
MPG123CONF += --with-default-audio=dummy --enable-buffer=no --enable-modules=no --disable-network

ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), armeabi-v7a)
MPG123CONF += --with-cpu=arm_fpu
else ifeq ($(ANDROID_ABI), arm64-v8a)
MPG123CONF += --with-cpu=aarch64
else
MPG123CONF += --with-cpu=generic_fpu
endif
endif

ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH), x86_64)
MPG123CONF += --with-cpu=generic_dither
endif
endif

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download_pkg,$(MPG123_URL),mpg123)

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	$(APPLY) $(SRC)/mpg123/no-programs.patch
	$(APPLY) $(SRC)/mpg123/mpg123-libm.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/mpg123/mpg123_android_off_t.patch
endif
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/mpg123/mpg123-win32.patch
endif
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/mpg123/winstore.patch
endif
	$(MOVE)

.mpg123: mpg123
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(MPG123CONF)
	cd $< && $(MAKE) install
	touch $@
