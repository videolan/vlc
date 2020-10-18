# rnnoise

RNNOISE_GITURL := http://github.com/xiph/rnnoise.git
RNNOISE_GITHASH := 90ec41ef659fd82cfec2103e9bb7fc235e9ea66c

ifndef HAVE_ANDROID
PKGS += rnnoise
endif

ifeq ($(call need_pkg,"rnnoise"),)
PKGS_FOUND += rnnoise
endif

$(TARBALLS)/rnnoise-$(RNNOISE_GITHASH).tar.xz:
	$(call download_git,$(RNNOISE_GITURL),,$(RNNOISE_GITHASH))

.sum-rnnoise: rnnoise-$(RNNOISE_GITHASH).tar.xz
	$(call check_githash,$(RNNOISE_GITHASH))
	touch $@

rnnoise: rnnoise-$(RNNOISE_GITHASH).tar.xz .sum-rnnoise
	$(UNPACK)
	$(MOVE)

.rnnoise: rnnoise
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-doc $(HOSTCONF)
	cd $< && $(MAKE)
	$(call pkg_static,"rnnoise.pc")
	cd $< && $(MAKE) install
	touch $@
