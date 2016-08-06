# shout

SHOUT_VERSION := 2.4.1
SHOUT_URL := http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz

ifdef BUILD_ENCODERS
ifdef BUILD_NETWORK
PKGS += shout
endif
endif
ifeq ($(call need_pkg,"shout >= 2.1"),)
PKGS_FOUND += shout
endif

$(TARBALLS)/libshout-$(SHOUT_VERSION).tar.gz:
	$(call download_pkg,$(SHOUT_URL),shout)

.sum-shout: libshout-$(SHOUT_VERSION).tar.gz

# TODO: fix socket stuff on POSIX and Linux
libshout: libshout-$(SHOUT_VERSION).tar.gz .sum-shout
	$(UNPACK)
	$(APPLY) $(SRC)/shout/bsd.patch
	$(APPLY) $(SRC)/shout/libshout-arpa.patch
	$(APPLY) $(SRC)/shout/fix-xiph_openssl.patch
	$(call pkg_static,"shout.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_shout = ogg $(DEPS_ogg) theora $(DEPS_theora) speex $(DEPS_speex)
DEPS_shout += vorbis $(DEPS_vorbis)

SHOUT_CONF :=

ifdef HAVE_WIN32
SHOUT_CONF += "--disable-thread"
endif
ifdef HAVE_ANDROID
SHOUT_CONF += "--disable-thread"
endif

.shout: libshout
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --without-openssl $(SHOUT_CONF) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
