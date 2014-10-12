# speexdsp

SPEEXDSP_VERSION := git
SPEEXDSP_HASH := HEAD
SPEEXDSP_GITURL := http://git.xiph.org/?p=speexdsp.git;a=snapshot;h=$(SPEEXDSP_HASH);sf=tgz

PKGS += speexdsp
ifeq ($(call need_pkg,"speexdsp"),)
PKGS_FOUND += speexdsp
endif

$(TARBALLS)/speexdsp-git.tar.gz:
	$(call download,$(SPEEXDSP_GITURL))

.sum-speexdsp: speexdsp-$(SPEEXDSP_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

speexdsp: speexdsp-$(SPEEXDSP_VERSION).tar.gz .sum-speexdsp
	rm -Rf $@-git $@
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(MOVE)

SPEEXDSP_CONF := --enable-resample-full-sinc-table --disable-examples
ifeq ($(ARCH),aarch64)
# old neon, not compatible with aarch64
SPEEXDSP_CONF += --disable-neon
endif
ifndef HAVE_FPU
SPEEXDSP_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEXDSP_CONF += --enable-arm5e-asm
endif
endif

.speexdsp: speexdsp
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SPEEXDSP_CONF)
	cd $< && $(MAKE) install
	touch $@
