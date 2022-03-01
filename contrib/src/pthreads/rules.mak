# winpthreads, dxvahd, winrt_headers, dcomp

MINGW64_VERSION := 9.0.0
MINGW64_URL := https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2/download
MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += pthreads

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += winrt_headers
PKGS_ALL += winrt_headers
endif # HAVE_WINSTORE
PKGS += dxvahd dcomp
PKGS_ALL += dxvahd dcomp
ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += dxvahd
ifdef HAVE_WINSTORE
PKGS_FOUND += winrt_headers
endif # HAVE_WINSTORE
endif # MINGW 8
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += pthreads
endif
ifneq ($(shell $(CC) $(CFLAGS) -E -dM -include dcomp.h - < /dev/null | grep -m 1 IDCompositionDevice3),)
PKGS_FOUND += dcomp
endif
endif # !HAVE_VISUALSTUDIO
endif # HAVE_WIN32

$(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

# .sum-pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2
.sum-pthreads: mingw-w64-$(MINGW64_HASH).tar.xz

# pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-pthreads
pthreads: mingw-w64-$(MINGW64_HASH).tar.xz .sum-pthreads
	$(UNPACK)
	$(MOVE)

.pthreads: pthreads
	cd $</mingw-w64-libraries/winpthreads && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) -C mingw-w64-libraries -C winpthreads install
	touch $@

.sum-winrt_headers: .sum-pthreads
	touch $@

.winrt_headers: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/windows.storage.h "$(PREFIX)/include"
	touch $@

.sum-dxvahd: .sum-pthreads
	touch $@

.dxvahd: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxvahd.h "$(PREFIX)/include"
	touch $@

.sum-dcomp: .sum-pthreads
	touch $@

.dcomp: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dcomp.h "$(PREFIX)/include"
	touch $@
