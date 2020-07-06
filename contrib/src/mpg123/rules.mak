# mpg123
MPG123_VERSION := 1.26.2
MPG123_URL := $(SF)/mpg123/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"libmpg123"),)
PKGS_FOUND += mpg123
endif

MPG123_CFLAGS := $(CFLAGS)
# Same forced value as in VLC
MPG123_CFLAGS += -D_FILE_OFFSET_BITS=64

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

ifdef HAVE_NACL
MPG123CONF += ac_cv_header_sys_select_h=no
endif

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download_pkg,$(MPG123_URL),mpg123)

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	# remove generated file from the source package
	cd $(UNPACK_DIR) && rm -rf src/libsyn123/syn123.h
	$(APPLY) $(SRC)/mpg123/no-programs.patch
	$(call pkg_static,"libmpg123.pc.in")
	$(MOVE)

.mpg123: mpg123
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(MPG123_CFLAGS)" ./configure $(MPG123CONF)
	cd $< && $(MAKE) install
	touch $@
