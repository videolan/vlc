MEDIALIBRARY_HASH := 16d4ad78bdbdb8122b57e48edd68279009f062c3
MEDIALIBRARY_VERSION := git-$(MEDIALIBRARY_HASH)
MEDIALIBRARY_GITURL := https://code.videolan.org/videolan/medialibrary.git

PKGS += medialibrary
ifeq ($(call need_pkg,"medialibrary >= 0.7.1"),)
PKGS_FOUND += medialibrary
endif

DEPS_medialibrary = sqlite $(DEPS_sqlite)

$(TARBALLS)/medialibrary-$(MEDIALIBRARY_VERSION).tar.xz:
	$(call download_git,$(MEDIALIBRARY_GITURL),,$(MEDIALIBRARY_HASH))

.sum-medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.xz
	$(call check_githash,$(MEDIALIBRARY_HASH))
	touch $@

medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.xz .sum-medialibrary
	rm -rf $@-$(MEDIALIBRARY_VERSION) $@
	mkdir -p $@-$(MEDIALIBRARY_VERSION)
	tar xvf "$<" --strip-components=1 -C $@-$(MEDIALIBRARY_VERSION)
	$(call pkg_static, "medialibrary.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.medialibrary: medialibrary
	$(RECONF)
	cd $< && $(HOSTVARS_PIC) ./configure --disable-tests --without-libvlc $(HOSTCONF)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@

