# winpthreads

WINPTHREADS_VERSION := 9.0.0
WINPTHREADS_URL := https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2/download
WINPTHREADS_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
WINPTHREADS_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += pthreads

ifndef HAVE_VISUALSTUDIO
PKGS += dxvahd
PKGS_ALL += dxvahd
ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += dxvahd
endif # MINGW 8
ifeq ($(HAVE_WINPTHREAD),)
PKGS_FOUND += pthreads
endif
endif # !HAVE_VISUALSTUDIO
endif # HAVE_WIN32

$(TARBALLS)/mingw-w64-$(WINPTHREADS_HASH).tar.xz:
	$(call download_git,$(WINPTHREADS_GITURL),,$(WINPTHREADS_HASH))

$(TARBALLS)/mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2:
	$(call download_pkg,$(WINPTHREADS_URL),winpthreads)

# .sum-pthreads: mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2
.sum-pthreads: mingw-w64-$(WINPTHREADS_HASH).tar.xz

# pthreads: mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2 .sum-pthreads
pthreads: mingw-w64-$(WINPTHREADS_HASH).tar.xz .sum-pthreads
	$(UNPACK)
	$(MOVE)

.pthreads: pthreads
	cd $</mingw-w64-libraries/winpthreads && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) -C mingw-w64-libraries -C winpthreads install
	touch $@

.sum-dxvahd: .sum-pthreads
	touch $@

.dxvahd: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxvahd.h "$(PREFIX)/include"
	touch $@
