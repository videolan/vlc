DAALA_VERSION := git
DAALA_HASH := HEAD
DAALA_GITURL := http://git.xiph.org/?p=daala.git;a=snapshot;h=$(DAALA_HASH);sf=tgz

# Default disabled for now
# PKGS += daala
ifeq ($(call need_pkg,"daala"),)
PKGS_FOUND += daala
endif

$(TARBALLS)/daala-git.tar.gz:
	$(call download,$(DAALA_GITURL))

.sum-daala: daala-$(DAALA_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

daala: daala-$(DAALA_VERSION).tar.gz .sum-daala
	rm -Rf $@-git $@
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(MOVE)
	mkdir -p $@/m4

DAALACONF := $(HOSTCONF) \
	--disable-player --disable-tools --disable-unit-tests

DEPS_daala = ogg $(DEPS_ogg)

.daala: daala
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(DAALACONF)
	cd $< && $(MAKE) install
	touch $@
