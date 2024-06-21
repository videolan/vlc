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
	$(MOVE)

TWOLAME_CONF := CFLAGS="${CFLAGS} -DLIBTWOLAME_STATIC"

.twolame: twolame
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(TWOLAME_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) -C libtwolame
	+$(MAKEBUILD) -C libtwolame install
	+$(MAKEBUILD) install-data
	touch $@
