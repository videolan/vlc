# shout

SHOUT_VERSION := 2.3.1
SHOUT_URL := http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz

ifdef BUILD_ENCODERS
PKGS += shout
endif
ifeq ($(call need_pkg,"shout >= 2.1"),)
PKGS_FOUND += shout
endif

$(TARBALLS)/libshout-$(SHOUT_VERSION).tar.gz:
	$(call download,$(SHOUT_URL))

.sum-shout: libshout-$(SHOUT_VERSION).tar.gz

# TODO: fix socket stuff on POSIX and Linux
libshout: libshout-$(SHOUT_VERSION).tar.gz .sum-shout
	$(UNPACK)
	$(APPLY) $(SRC)/shout/libshout-win32.patch
	$(APPLY) $(SRC)/shout/bsd.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_shout = ogg $(DEPS_ogg) theora $(DEPS_theora) speex $(DEPS_speex)
DEPS_shout += vorbis $(DEPS_vorbis)

SHOUT_CONF :=

ifdef HAVE_WIN32
SHOUT_CONF += "--disable-thread"
endif

.shout: libshout
	cd $< && $(HOSTVARS) ./configure $(SHOUT_CONF) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
