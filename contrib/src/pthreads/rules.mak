# winpthreads, dxvahd

MINGW64_VERSION := 9.0.0
MINGW64_URL := https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v$(MINGW64_VERSION).tar.bz2/download
MINGW64_HASH=2c35e8ff0d33916bd490e8932cba2049cd1af3d0
MINGW64_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += pthreads

ifndef HAVE_VISUALSTUDIO
PKGS += dxva dxvahd
PKGS_ALL += dxva dxvahd
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

# .sum-pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2
.sum-pthreads: mingw-w64-$(MINGW64_HASH).tar.xz

# pthreads: mingw-w64-v$(MINGW64_VERSION).tar.bz2 .sum-pthreads
pthreads: mingw-w64-$(MINGW64_HASH).tar.xz .sum-pthreads
	$(UNPACK)
	$(APPLY) $(SRC)/pthreads/0001-headers-Update-to-Wine-master-and-regenerate-H-from-.patch
	$(APPLY) $(SRC)/pthreads/0002-headers-dxvahd-Regenerate-H-from-IDL.patch
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

.sum-dxva: .sum-pthreads
	touch $@

.dxva: pthreads
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp mingw-w64-headers/include/dxva.h "$(PREFIX)/include"
	touch $@

