# mpg123
MPG123_VERSION := 1.22.4
MPG123_URL := $(SF)/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"mpg123"),)
PKGS_FOUND += mpg123
endif

MPG123CONF = $(HOSTCONF)

ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
MPG123CONF += --with-cpu=generic_fpu
endif
endif
ifdef HAVE_VISUALSTUDIO
ifeq ($(ARCH), x86_64)
MPG123CONF += --with-cpu=generic_dither
endif
endif

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download,$(MPG123_URL))

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	$(APPLY) $(SRC)/mpg123/no-programs.patch
	$(APPLY) $(SRC)/mpg123/mpg123-libm.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/mpg123/mpg123_android_off_t.patch
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
