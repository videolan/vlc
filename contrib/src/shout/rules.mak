# shout

SHOUT_VERSION := 2.2.2
SHOUT_URL := http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz

PKGS += shout

$(TARBALLS)/libshout-$(SHOUT_VERSION).tar.gz:
	$(call download,$(SHOUT_URL))

.sum-shout: libshout-$(SHOUT_VERSION).tar.gz

# TODO: fix socket stuff on POSIX and Linux
libshout: libshout-$(SHOUT_VERSION).tar.gz .sum-shout
	$(UNPACK)
	$(APPLY) $(SRC)/shout/libshout-win32.patch
	$(MOVE)

ifdef HAVE_FPU
.shout: .vorbis
else
.shout: .tremor
endif

.shout: libshout .theora .ogg .speex
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
