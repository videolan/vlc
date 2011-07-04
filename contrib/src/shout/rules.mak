# shout

SHOUT_VERSION := 2.2.2
SHOUT_URL := http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz

PKGS += shout
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
	$(MOVE)

DEPS_shout = ogg $(DEPS_ogg) theora $(DEPS_theora) speex $(DEPS_speex)
ifdef HAVE_FPU
DEPS_shout += vorbis $(DEPS_vorbis)
else
DEPS_shout += tremor $(DEPS_tremor)
endif

.shout: libshout
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
