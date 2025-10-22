# twolame

TWOLAME_VERSION := 0.4.0
TWOLAME_URL := $(SF)/twolame/twolame-$(TWOLAME_VERSION).tar.gz

ifdef BUILD_ENCODERS
PKGS += twolame
endif
ifeq ($(call need_pkg,"twolame"),)
PKGS_FOUND += twolame
endif

$(TARBALLS)/twolame-$(TWOLAME_VERSION).tar.gz:
	$(call download_pkg,$(TWOLAME_URL),twolame)

.sum-twolame: twolame-$(TWOLAME_VERSION).tar.gz

twolame: twolame-$(TWOLAME_VERSION).tar.gz .sum-twolame
	$(UNPACK)
	$(call update_autoconfig,build-scripts)
	$(call pkg_static,"twolame.pc.in")
	$(MOVE)

TWOLAME_CONF := --enable-static

.twolame: twolame
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(TWOLAME_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) -C libtwolame
	+$(MAKEBUILD) -C libtwolame install
	+$(MAKEBUILD) install-data
	touch $@
