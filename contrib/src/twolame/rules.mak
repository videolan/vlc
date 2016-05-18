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
	$(call download_pkg,$(TWOLAME_URL),twolame)

.sum-twolame: twolame-$(TWOLAME_VERSION).tar.gz

twolame: twolame-$(TWOLAME_VERSION).tar.gz .sum-twolame
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && cp config.guess config.sub build-scripts
	$(MOVE)

.twolame: twolame
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="${CFLAGS} -DLIBTWOLAME_STATIC" ./configure $(HOSTCONF)
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C libtwolame install
	cd $< && $(MAKE) install-data
	touch $@
