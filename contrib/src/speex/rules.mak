# speex

#SPEEX_VERSION := 1.2rc1
#SPEEX_URL := http://downloads.us.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz
SPEEX_VERSION := git
SPEEX_HASH := a6d05eb
#SPEEX_HASH := HEAD
SPEEX_GITURL := http://git.xiph.org/?p=speex.git;a=snapshot;h=$(SPEEX_HASH);sf=tgz

PKGS += speex
ifeq ($(call need_pkg,"speex >= 1.0.5"),)
PKGS_FOUND += speex
endif

#$(TARBALLS)/speex-$(SPEEX_VERSION).tar.gz:
#	$(call download,$(SPEEX_URL))

$(TARBALLS)/speex-git.tar.gz:
	$(call download,$(SPEEX_GITURL))

.sum-speex: speex-$(SPEEX_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

speex: speex-$(SPEEX_VERSION).tar.gz .sum-speex
	rm -Rf $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(APPLY) $(SRC)/speex/no-ogg.patch
	$(APPLY) $(SRC)/speex/automake.patch
	$(APPLY) $(SRC)/speex/neon.patch
	$(MOVE)

SPEEX_CONF := --without-ogg --enable-resample-full-sinc-table
ifndef HAVE_FPU
SPEEX_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEX_CONF += --enable-arm5e-asm
endif
endif

.speex: speex
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SPEEX_CONF)
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
