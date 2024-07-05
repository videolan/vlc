MEDIALIBRARY_VERSION := 0.13.0
MEDIALIBRARY_URL := https://code.videolan.org/videolan/medialibrary/-/archive/$(MEDIALIBRARY_VERSION)/medialibrary-$(MEDIALIBRARY_VERSION).tar.bz2

PKGS += medialibrary
ifeq ($(call need_pkg,"medialibrary >= 0.13.0"),)
PKGS_FOUND += medialibrary
endif

DEPS_medialibrary = sqlite $(DEPS_sqlite)

$(TARBALLS)/medialibrary-$(MEDIALIBRARY_VERSION).tar.bz2:
	$(call download_pkg,$(MEDIALIBRARY_URL),medialibrary)

.sum-medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.bz2

medialibrary: medialibrary-$(MEDIALIBRARY_VERSION).tar.bz2 .sum-medialibrary
	$(UNPACK)
	$(APPLY) $(SRC)/medialibrary/Fix-CacheWorker.patch
	$(APPLY) $(SRC)/medialibrary/0002-win32-DeviceLister-don-t-use-wchar_t-string-if-they-.patch
	$(APPLY) $(SRC)/medialibrary/0003-utils-Directory-don-t-use-wchar_t-string-if-they-are.patch
	$(APPLY) $(SRC)/medialibrary/0004-test-common-don-t-use-wchar_t-string-if-they-are-NUL.patch
	$(APPLY) $(SRC)/medialibrary/0005-LockFile-don-t-use-char-string-if-they-are-NULL.patch
	$(APPLY) $(SRC)/medialibrary/0006-fs-Directory-don-t-use-char-string-if-they-are-NULL.patch
	$(APPLY) $(SRC)/medialibrary/0007-utils-Directory-don-t-use-char-string-if-they-are-NU.patch
	$(MOVE)

.medialibrary: medialibrary crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) -Dlibvlc=disabled -Dlibtool_workaround=true
	+$(MESONBUILD)
	touch $@

