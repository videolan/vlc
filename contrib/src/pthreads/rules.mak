# winpthreads, dxvahd, winrt_headers, dcomp

MINGW64_VERSION := 10.0.0
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
PKGS += d3d9 dxva dxvahd dcomp
PKGS_ALL += d3d9 dxva dxvahd dcomp
ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += d3d9 dxvahd
ifdef HAVE_WINSTORE
PKGS_FOUND += winrt_headers
endif # HAVE_WINSTORE
endif # MINGW 8
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dxva
endif # MINGW 10
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += pthreads
endif
ifeq ($(call mingw_at_least, 10), true)
PKGS_FOUND += dcomp
endif
endif # !HAVE_VISUALSTUDIO
endif # HAVE_WIN32

$(TARBALLS)/mingw-w64-$(MINGW64_HASH).tar.xz:
	$(call download_git,$(MINGW64_GITURL),,$(MINGW64_HASH))

$(TARBALLS)/mingw-w64-v$(MINGW64_VERSION).tar.bz2:
	$(call download_pkg,$(MINGW64_URL),winpthreads)

.sum-pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2
# .sum-pthreads: mingw-w64-$(MINGW64_HASH).tar.xz

pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-pthreads
# pthreads: mingw-w64-$(MINGW64_HASH).tar.xz .sum-pthreads
	$(UNPACK)
	$(MOVE)

.pthreads: pthreads
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../mingw-w64-libraries/winpthreads/configure $(HOSTCONF)
	$(MAKE) -C $</_build install
	touch $@

.sum-winrt_headers: .sum-pthreads
	touch $@

.winrt_headers: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/windows.storage.h "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/eventtoken.h "$(PREFIX)/include"
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

.sum-d3d9: .sum-pthreads
	touch $@

.d3d9: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/d3d9.h "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/d3d9caps.h "$(PREFIX)/include"
	touch $@

.sum-dxva: .sum-pthreads
	touch $@

.dxva: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
	touch $@

