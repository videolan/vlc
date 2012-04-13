# twolame

TWOLAME_VERSION := 0.3.13
TWOLAME_URL := $(SF)/twolame/twolame-$(TWOLAME_VERSION).tar.gz

ifdef BUILD_ENCODERS
PKGS += twolame
endif
ifeq ($(call need_pkg,"twolame"),)
PKGS_FOUND += twolame
endif

$(TARBALLS)/twolame-$(TWOLAME_VERSION).tar.gz:
	$(call download,$(TWOLAME_URL))

$(TARBALLS)/twolame-winutil.h:
	$(call download,"http://twolame.svn.sourceforge.net/viewvc/*checkout*/twolame/trunk/win32/winutil.h")

.sum-twolame: twolame-$(TWOLAME_VERSION).tar.gz twolame-winutil.h

twolame: twolame-$(TWOLAME_VERSION).tar.gz twolame-winutil.h .sum-twolame
	$(UNPACK)
ifdef HAVE_WIN32
	cp -f $(filter %winutil.h,$^) $@-$(TWOLAME_VERSION)/win32/winutil.h
endif
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && cp config.guess config.sub build-scripts
	$(MOVE)

.twolame: twolame
	cd $< && $(HOSTVARS) CFLAGS="${CFLAGS} -DLIBTWOLAME_STATIC" ./configure $(HOSTCONF)
	cd $< && $(MAKE)
	cd $</libtwolame && $(MAKE) install
	cd $< && $(MAKE) install-data
	touch $@
