# winpthreads, dxvahd, winrt_headers, dcomp

MINGW64_VERSION := 11.0.0
MINGW64_URL := $(SF)/mingw-w64/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2
MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += winpthreads

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += winrt_headers alloweduwp
else  # !HAVE_WINSTORE
PKGS += d3d9 dcomp
endif # !HAVE_WINSTORE
PKGS += dxva dxvahd mingw11-fixes

ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += d3d9
endif # MINGW 8
ifeq ($(call mingw_at_least, 9), true)
ifdef HAVE_WINSTORE
PKGS_FOUND += winrt_headers
endif # HAVE_WINSTORE
endif # MINGW 9
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dxva
endif # MINGW 10
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dcomp
endif
ifeq ($(call mingw_at_least, 11), true)
PKGS_FOUND += dxvahd
endif # MINGW 11
ifeq ($(call mingw_at_least, 12), true)
PKGS_FOUND += mingw11-fixes
endif # MINGW 12
endif # !HAVE_VISUALSTUDIO

HAVE_WINPTHREAD := $(shell $(CC) $(CFLAGS) -E -dM -include pthread.h - < /dev/null >/dev/null 2>&1 || echo FAIL)
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += winpthreads
endif

endif # HAVE_WIN32

PKGS_ALL += winpthreads winrt_headers d3d9 dxva dxvahd dcomp mingw11-fixes alloweduwp

$(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

.sum-mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2
# .sum-mingw64: mingw-w64-$(MINGW64_HASH).tar.xz

mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-mingw64
# mingw64: mingw-w64-$(MINGW64_HASH).tar.xz .sum-mingw64
	$(UNPACK)
	$(APPLY) $(SRC)/mingw64/0001-headers-enable-GetFileInformationByHandle-in-Win10-U.patch
	$(APPLY) $(SRC)/mingw64/0002-headers-enable-VirtualAlloc-Ex-in-Win10-UWP-builds.patch
	$(APPLY) $(SRC)/mingw64/0003-headers-enable-GetVolumePathNameW-in-Win10-UWP-build.patch
	$(APPLY) $(SRC)/mingw64/0004-headers-enable-GET_MODULE_HANDLE_EX_xxx-defines-in-U.patch
	$(APPLY) $(SRC)/mingw64/0005-headers-enable-CreateHardLinkW-in-Win10-19H1-UWP-bui.patch
	$(APPLY) $(SRC)/mingw64/0006-headers-enable-more-module-API-in-Win10-19H1-UWP-bui.patch
	$(APPLY) $(SRC)/mingw64/0007-crt-add-api-ms-core-registry-def-files.patch
	$(APPLY) $(SRC)/mingw64/0008-headers-enable-some-Registry-API-calls-in-Win10-19H1.patch
	$(APPLY) $(SRC)/mingw64/0009-headers-only-enable-GetFileInformationByHandle-for-1.patch
	$(APPLY) $(SRC)/mingw64/0010-headers-allow-Get-SetHandleInformation-in-Win10-19H1.patch
	$(APPLY) $(SRC)/mingw64/0011-crt-add-missing-api-ms-win-core-heap-l1-1-0.patch
	$(APPLY) $(SRC)/mingw64/0012-headers-Allow-some-Heap-API-in-Win10-19H1-UWP-builds.patch
	$(APPLY) $(SRC)/mingw64/0013-headers-enable-FindResourceW-in-Win10-19H1-UWP-build.patch
	$(APPLY) $(SRC)/mingw64/0014-headers-check-which-version-of-UWP-Windows-contains-.patch
	$(APPLY) $(SRC)/mingw64/0015-headers-enabled-LoadLibraryEx-flags-in-Win10-19H1-UW.patch
	$(APPLY) $(SRC)/mingw64/0016-headers-Allow-SetDllDirectoryW-A-API-in-Win10-19H1-U.patch
	$(APPLY) $(SRC)/mingw64/0017-headers-allow-FORMAT_MESSAGE_ALLOCATE_BUFFER-in-UWP.patch
	$(APPLY) $(SRC)/mingw64/0018-headers-allow-RtlSecureZeroMemory-in-all-targets.patch
	$(APPLY) $(SRC)/mingw64/0019-headers-use-inline-version-of-RtlSecureZeroMemory-fo.patch
	$(APPLY) $(SRC)/mingw64/0001-headers-allow-CryptAcquireContext-in-Win10-RS4-UWP-b.patch
	$(APPLY) $(SRC)/mingw64/0002-headers-allow-CryptGenRandom-in-Win10-19H1-UWP-build.patch
	$(APPLY) $(SRC)/mingw64/0003-headers-allow-more-wincrypt-API-s-in-Win10-RS4-UWP-b.patch
	$(APPLY) $(SRC)/mingw64/0004-headers-allow-more-wincrypt-API-s-in-Win10-19H1-UWP-.patch
	$(APPLY) $(SRC)/mingw64/0005-crt-use-wincrypt-API-from-windowsapp-in-Windows-10.patch
	$(APPLY) $(SRC)/mingw64/0001-include-process-fix-bare-DllMain-_CRT_INIT-signature.patch
	$(MOVE)

.mingw64: mingw64
	touch $@

.sum-winpthreads: .sum-mingw64
	touch $@

.winpthreads: mingw64
	$(MAKEBUILDDIR)
	$(MAKECONFDIR)/mingw-w64-libraries/winpthreads/configure $(HOSTCONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@

.sum-winrt_headers: .sum-mingw64
	touch $@

MINGW_HEADERS_WINRT := \
    windows.foundation.h \
    windows.storage.h \
    windows.storage.streams.h \
    windows.system.threading.h \
    windows.foundation.collections.h \
    eventtoken.h \
    asyncinfo.h \
    windowscontracts.h

.winrt_headers: mingw64
	install -d "$(PREFIX)/include"
	install $(addprefix $</mingw-w64-headers/include/,$(MINGW_HEADERS_WINRT)) "$(PREFIX)/include"
	touch $@

.sum-dxvahd: .sum-mingw64
	touch $@

.dxvahd: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dxvahd.h "$(PREFIX)/include"
	touch $@

.sum-mingw11-fixes: .sum-mingw64
	touch $@

.mingw11-fixes: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/crt/process.h "$(PREFIX)/include"
	touch $@

.sum-dcomp: .sum-mingw64
	touch $@

.dcomp: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dcomp.h "$(PREFIX)/include"
	touch $@

.sum-d3d9: .sum-mingw64
	touch $@

MINGW_HEADERS_D3D9 := d3d9.h d3d9caps.h

.d3d9: mingw64
	install -d "$(PREFIX)/include"
	install $(addprefix $</mingw-w64-headers/include/,$(MINGW_HEADERS_D3D9)) "$(PREFIX)/include"
	touch $@

.sum-dxva: .sum-mingw64
	touch $@

.dxva: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
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

.sum-alloweduwp: .sum-mingw64
	touch $@

.alloweduwp: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/fileapi.h      "$(PREFIX)/include"
	install $</mingw-w64-headers/include/memoryapi.h    "$(PREFIX)/include"
	install $</mingw-w64-headers/include/winbase.h      "$(PREFIX)/include"
	install $</mingw-w64-headers/include/libloaderapi.h "$(PREFIX)/include"
	install $</mingw-w64-headers/include/winreg.h       "$(PREFIX)/include"
	install $</mingw-w64-headers/include/handleapi.h    "$(PREFIX)/include"
	install $</mingw-w64-headers/include/wincrypt.h     "$(PREFIX)/include"
	install $</mingw-w64-headers/include/winnt.h        "$(PREFIX)/include"

	# Trick mingw-w64 into just building libwindowsapp.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_UWP_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a install-$(MINGW64_BUILDDIR)LIBRARIES
	touch $@

