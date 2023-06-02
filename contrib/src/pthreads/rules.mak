# winpthreads, dxvahd

MINGW64_VERSION := 10.0.0
MINGW64_URL := $(SF)/mingw-w64/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2
MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += pthreads

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += alloweduwp
endif
PKGS += dxva dxvahd
PKGS_ALL += dxva dxvahd alloweduwp
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dxva
endif # MINGW 10
ifeq ($(call mingw_at_least, 11), true)
PKGS_FOUND += dxvahd
endif # MINGW 11
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += pthreads
endif
endif # !HAVE_VISUALSTUDIO
endif # HAVE_WIN32

$(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

.sum-pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2
# .sum-pthreads: mingw-w64-$(MINGW64_HASH).tar.xz
# 	$(call check_githash,$(MINGW64_HASH))
# 	touch $@

pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-pthreads
# pthreads: mingw-w64-$(MINGW64_HASH).tar.xz .sum-pthreads
	$(UNPACK)
	$(APPLY) $(SRC)/pthreads/0001-headers-Update-to-Wine-master-and-regenerate-H-from-.patch
	$(APPLY) $(SRC)/pthreads/0002-headers-dxvahd-Regenerate-H-from-IDL.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-GetFileInformationByHandle-in-Win10-U.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-VirtualAlloc-Ex-in-Win10-UWP-builds.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-CreateHardLinkW-in-Win10-UWP-builds.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-GetVolumePathNameW-in-Win10-UWP-build.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-more-module-API-in-Win10-UWP-builds.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-GET_MODULE_HANDLE_EX_xxx-defines-in-U.patch
	$(APPLY) $(SRC)/pthreads/0001-headers-enable-some-Registry-API-calls-in-UWP-8.1-bu.patch
	$(APPLY) $(SRC)/pthreads/0001-add-api-ms-core-registry-def-files.patch
	$(MOVE)

.pthreads: pthreads
	cd $</mingw-w64-libraries/winpthreads && $(HOSTVARS) ./configure $(HOSTCONF)
	$(MAKE) -C $< -C mingw-w64-libraries -C winpthreads install
	touch $@

.sum-dxvahd: .sum-pthreads
	touch $@

.dxvahd: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxvahd.h "$(PREFIX)/include"
	touch $@

.sum-dxva: .sum-pthreads
	touch $@

.dxva: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
	touch $@


MINGW64_UWP_CONF := --without-headers --with-crt --without-libraries --without-tools
ifeq ($(ARCH),x86_64)
MINGW64_UWP_CONF +=--disable-lib32 --enable-lib64
MINGW64_BUILDDIR := lib64
else ifeq ($(ARCH),i386)
MINGW64_UWP_CONF +=--enable-lib32 --disable-lib64
MINGW64_BUILDDIR := lib32
else ifeq ($(ARCH),aarch64)
MINGW64_UWP_CONF +=--disable-lib32 --disable-lib64 --enable-libarm64
MINGW64_BUILDDIR := libarm64
else ifeq ($(ARCH),arm)
MINGW64_UWP_CONF +=--disable-lib32 --disable-lib64 --enable-libarm32
MINGW64_BUILDDIR := libarm32
endif

.sum-alloweduwp: .sum-pthreads
	touch $@

.alloweduwp: pthreads
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/fileapi.h "$(PREFIX)/include"
	install $</mingw-w64-headers/include/memoryapi.h "$(PREFIX)/include"
	install $</mingw-w64-headers/include/winbase.h "$(PREFIX)/include"
	install $</mingw-w64-headers/include/libloaderapi.h "$(PREFIX)/include"
	install $</mingw-w64-headers/include/winreg.h "$(PREFIX)/include"

	# Trick mingw-w64 into just building libwindowsapp.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_UWP_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a install-$(MINGW64_BUILDDIR)LIBRARIES
	touch $@
