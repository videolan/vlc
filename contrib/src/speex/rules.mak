# speex

SPEEX_VERSION := 1.2rc1
SPEEX_URL := http://downloads.us.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz

PKGS += speex
ifeq ($(call need_pkg,"speex >= 1.0.5"),)
PKGS_FOUND += speex
endif

$(TARBALLS)/speex-$(SPEEX_VERSION).tar.gz:
	$(call download,$(SPEEX_URL))

.sum-speex: speex-$(SPEEX_VERSION).tar.gz

speex: speex-$(SPEEX_VERSION).tar.gz .sum-speex
	$(UNPACK)
	$(MOVE)

# TODO: fixed point and ASM opts

.speex: speex
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --without-ogg
	cd $< && $(MAKE) install
	touch $@

# Speex DSP

PKGS += speexdsp
PKGS_ALL += speexdsp
ifeq ($(call need_pkg,"speexdsp"),)
PKGS_FOUND += speexdsp
endif

.sum-speexdsp: .sum-speex
	touch -r $< $@

DEPS_speexdsp = speex $(DEPS_speex)

.speexdsp:
	touch $@
