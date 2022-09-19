# rnnoise

RNNOISE_GITURL := $(GITHUB)/xiph/rnnoise.git
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

RNNOISE_CONF := --disable-examples --disable-doc

.rnnoise: rnnoise
	$(RECONF)
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF) $(RNNOISE_CONF)
	$(MAKE) -C $</_build
	$(call pkg_static,"_build/rnnoise.pc")
	$(MAKE) -C $</_build install
	touch $@
