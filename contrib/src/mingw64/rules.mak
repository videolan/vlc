# winpthreads, dxvahd, winrt_headers, dcomp

MINGW64_VERSION := 13.0.0
MINGW64_URL := $(SF)/mingw-w64/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2
# MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
# MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += winpthreads

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += winrt_headers alloweduwp
else  # !HAVE_WINSTORE
PKGS += dcomp
endif # !HAVE_WINSTORE
PKGS += dxva dxvahd mingw11-fixes mingw12-fixes mft10 d3d12 uiautomationcore
ifeq ($(ARCH),i386)
PKGS += dxva_x86
endif

ifdef HAVE_WINSTORE
PKGS_FOUND += winrt_headers
endif # HAVE_WINSTORE
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dxva mft10
endif # MINGW 10
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dcomp
endif
ifeq ($(call mingw_at_least, 11), true)
PKGS_FOUND += uiautomationcore
endif # MINGW 11
ifeq ($(call mingw_at_least, 12), true)
PKGS_FOUND += mingw11-fixes d3d12
endif # MINGW 12
ifeq ($(call mingw_at_least, 13), true)
PKGS_FOUND += mingw12-fixes dxvahd
ifeq ($(ARCH),i386)
PKGS_FOUND += dxva_x86
endif
endif # MINGW 13
endif # !HAVE_VISUALSTUDIO

HAVE_WINPTHREAD := $(shell $(CC) $(CFLAGS) -E -dM -include pthread.h - < /dev/null >/dev/null 2>&1 || echo FAIL)
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += winpthreads
endif

endif # HAVE_WIN32

PKGS_ALL += winpthreads winrt_headers dxva dxvahd dxva_x86 dcomp mingw11-fixes mingw12-fixes alloweduwp mft10 d3d12 uiautomationcore

# $(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
# 	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

.sum-mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2
# .sum-mingw64: mingw-w64-$(MINGW64_HASH).tar.xz

mingw64: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-mingw64
# mingw64: mingw-w64-$(MINGW64_HASH).tar.xz .sum-mingw64
	$(UNPACK)
	$(APPLY) $(SRC)/mingw64/0001-winnt-define-__MINGW_CXX1-1-4-_CONSTEXPR-found-in-_m.patch
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

.sum-mingw12-fixes: .sum-mingw64
	touch $@

.mingw12-fixes: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/strmif.h "$(PREFIX)/include"
	touch $@

.sum-dcomp: .sum-mingw64
	touch $@

.dcomp: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dcomp.h "$(PREFIX)/include"
	touch $@

.sum-mft10: .sum-mingw64
	touch $@

MINGW_HEADERS_MFT := mfidl.h mfapi.h mftransform.h mferror.h mfobjects.h mmreg.h

.mft10: mingw64
	install -d "$(PREFIX)/include"
	install $(addprefix $</mingw-w64-headers/include/,$(MINGW_HEADERS_MFT)) "$(PREFIX)/include"
	touch $@

.sum-dxva: .sum-mingw64
	touch $@

.dxva: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
	touch $@

MINGW64_MINIMALCRT_CONF := --without-headers --with-crt --without-libraries --without-tools
ifeq ($(ARCH),x86_64)
MINGW64_MINIMALCRT_CONF +=--disable-lib32 --enable-lib64
MINGW64_BUILDDIR := lib64
else ifeq ($(ARCH),i386)
MINGW64_MINIMALCRT_CONF +=--enable-lib32 --disable-lib64
MINGW64_BUILDDIR := lib32
else ifeq ($(ARCH),aarch64)
MINGW64_MINIMALCRT_CONF +=--disable-lib32 --disable-lib64 --enable-libarm64
MINGW64_BUILDDIR := libarm64
else ifeq ($(ARCH),arm)
MINGW64_MINIMALCRT_CONF +=--disable-lib32 --disable-lib64 --enable-libarm32
MINGW64_BUILDDIR := libarm32
endif

.sum-alloweduwp: .sum-mingw64
	touch $@

.alloweduwp: BUILD_DIR=$</vlc_build_alloweduwp
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
	install $</mingw-w64-headers/include/heapapi.h      "$(PREFIX)/include"
	install $</mingw-w64-headers/include/minwinbase.h   "$(PREFIX)/include"

	# Trick mingw-w64 into just building libwindowsapp.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_MINIMALCRT_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libwindowsapp.a install-$(MINGW64_BUILDDIR)LIBRARIES
	touch $@

.sum-d3d12: .sum-mingw64
	touch $@

.d3d12: BUILD_DIR=$</vlc_build_d3d12
.d3d12: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/d3d12.h             "$(PREFIX)/include"
	install $</mingw-w64-headers/include/d3d12sdklayers.h    "$(PREFIX)/include"
	install $</mingw-w64-headers/include/d3d12shader.h       "$(PREFIX)/include"

	# Trick mingw-w64 into just building libd3d12.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_MINIMALCRT_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libd3d12.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libd3d12.a install-$(MINGW64_BUILDDIR)LIBRARIES
	touch $@

.sum-uiautomationcore: .sum-mingw64
	touch $@

.uiautomationcore: BUILD_DIR=$</vlc_build_uiautomationcore
.uiautomationcore: mingw64
	install -d "$(PREFIX)/include"
	install $</mingw-w64-headers/include/uiautomation.h           "$(PREFIX)/include"
	install $</mingw-w64-headers/include/uiautomationcore.h       "$(PREFIX)/include"
	install $</mingw-w64-headers/include/uiautomationcoreapi.h    "$(PREFIX)/include"
	install $</mingw-w64-headers/include/uiautomationclient.h     "$(PREFIX)/include"

	# Trick mingw-w64 into just building libuiautomationcore.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_MINIMALCRT_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libuiautomationcore.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libuiautomationcore.a install-$(MINGW64_BUILDDIR)LIBRARIES
	touch $@

.sum-dxva_x86: .sum-mingw64
	touch $@

.dxva_x86: BUILD_DIR=$</vlc_build_dxva_x86
.dxva_x86: mingw64
ifeq ($(ARCH),i386)
	install -d "$(PREFIX)/include"

	# Trick mingw-w64 into just building libdxva2.a
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MINGW64_MINIMALCRT_CONF)
	mkdir -p $(BUILD_DIR)/mingw-w64-crt/$(MINGW64_BUILDDIR)
	+$(MAKEBUILD) -C mingw-w64-crt LIBRARIES=$(MINGW64_BUILDDIR)/libdxva2.a DATA= HEADERS=
	+$(MAKEBUILD) -C mingw-w64-crt $(MINGW64_BUILDDIR)_LIBRARIES=$(MINGW64_BUILDDIR)/libdxva2.a install-$(MINGW64_BUILDDIR)LIBRARIES
endif
	touch $@
