MEDIALIBRARY_HASH := 363bb8fcf50851a0ce7438ee5a9ad35f6f8954d0
MEDIALIBRARY_VERSION := git-$(MEDIALIBRARY_HASH)
MEDIALIBRARY_GITURL := https://code.videolan.org/videolan/medialibrary.git

PKGS += medialibrary
ifeq ($(call need_pkg,"medialibrary >= 0.9.3"),)
PKGS_FOUND += medialibrary
endif

DEPS_medialibrary = sqlite $(DEPS_sqlite)

$(TARBALLS)/medialibrary-$(MEDIALIBRARY_VERSION).tar.xz:
	$(call download_git,$(MEDIALIBRARY_GITURL),,$(MEDIALIBRARY_HASH))

.sum-medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.xz
	$(call check_githash,$(MEDIALIBRARY_HASH))
	touch $@

medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.xz .sum-medialibrary
	$(UNPACK)
	$(MOVE)

.medialibrary: medialibrary
	cd $< && $(HOSTVARS_MESON) $(MESON) -Dlibvlc=disabled build
	cd $< && cd build && ninja install
	touch $@

