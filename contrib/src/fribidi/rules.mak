# FRIBIDI
FRIBIDI_VERSION := 0.19.6
FRIBIDI_URL := http://fribidi.org/download/fribidi-$(FRIBIDI_VERSION).tar.bz2

PKGS += fribidi
ifeq ($(call need_pkg,"fribidi"),)
PKGS_FOUND += fribidi
endif

$(TARBALLS)/fribidi-$(FRIBIDI_VERSION).tar.bz2:
	$(call download,$(FRIBIDI_URL))

.sum-fribidi: fribidi-$(FRIBIDI_VERSION).tar.bz2

fribidi: fribidi-$(FRIBIDI_VERSION).tar.bz2 .sum-fribidi
	$(UNPACK)
	$(APPLY) $(SRC)/fribidi/fribidi.patch
	$(APPLY) $(SRC)/fribidi/no-ansi.patch
	$(MOVE)

# FIXME: DEPS_fribidi = iconv $(DEPS_iconv)
.fribidi: fribidi
	cd $< && rm -f configure && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
