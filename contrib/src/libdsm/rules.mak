# libdsm

#LIBDSM_GITURL := git://github.com/videolabs/libdsm.git
LIBDSM_VERSION := 0.3.0
LIBDSM_URL := https://github.com/videolabs/libdsm/releases/download/v$(LIBDSM_VERSION)/libdsm-$(LIBDSM_VERSION).tar.gz

ifeq ($(call need_pkg,"libdsm >= 0.2.0"),)
PKGS_FOUND += libdsm
endif

$(TARBALLS)/libdsm-$(LIBDSM_VERSION).tar.gz:
	$(call download_pkg,$(LIBDSM_URL),libdsm)

LIBDSM_CONF = $(HOSTCONF)

ifndef WITH_OPTIMIZATION
LIBDSM_CONF += --enable-debug
endif
.sum-libdsm: libdsm-$(LIBDSM_VERSION).tar.gz

libdsm: libdsm-$(LIBDSM_VERSION).tar.gz .sum-libdsm
	$(UNPACK)
	$(MOVE)

DEPS_libdsm = libtasn1 iconv

.libdsm: libdsm
	$(RECONF)
	cd $< && $(HOSTVARS_PIC) ./configure --disable-programs $(LIBDSM_CONF)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh libdsm.pc
	cd $< && $(MAKE) install
	touch $@
