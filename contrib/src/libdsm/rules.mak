# libdsm

#LIBDSM_GITURL := git://github.com/videolabs/libdsm.git
LIBDSM_VERSION := 0.0.6
LIBDSM_URL := https://github.com/videolabs/libdsm/releases/download/v$(LIBDSM_VERSION)/libdsm-$(LIBDSM_VERSION).tar.gz

ifeq ($(call need_pkg,"libdsm >= 0.0.4"),)
PKGS_FOUND += libdsm
endif

$(TARBALLS)/libdsm-$(LIBDSM_VERSION).tar.gz:
	$(call download,$(LIBDSM_URL))

.sum-libdsm: libdsm-$(LIBDSM_VERSION).tar.gz

libdsm: libdsm-$(LIBDSM_VERSION).tar.gz .sum-libdsm
	$(UNPACK)
	$(MOVE)

DEPS_libdsm = libtasn1 iconv

.libdsm: libdsm
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
