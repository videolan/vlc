# winpthreads

WINPTHREADS_VERSION := 7.0.0
WINPTHREADS_URL := https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2/download
WINPTHREADS_HASH=a32b622261b490ec4e4f675dfef010d1274c6c4d
WINPTHREADS_GITURL := https://git.code.sf.net/p/mingw-w64/mingw-w64

ifdef HAVE_WIN32
PKGS += pthreads
endif

ifndef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
PKGS += winrt_headers
PKGS_ALL += winrt_headers
endif
endif
ifeq ($(HAVE_MINGW64_V8),true)
PKGS_FOUND += winrt_headers
endif

$(TARBALLS)/mingw-w64-$(WINPTHREADS_HASH).tar.xz:
	$(call download_git,$(WINPTHREADS_GITURL),,$(WINPTHREADS_HASH))

$(TARBALLS)/mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2:
	$(call download_pkg,$(WINPTHREADS_URL),winpthreads)

# .sum-pthreads: mingw-w64-v$(WINPTHREADS_VERSION).tar.bz2
.sum-pthreads: mingw-w64-$(WINPTHREADS_HASH).tar.xz

pthreads: mingw-w64-$(WINPTHREADS_HASH).tar.xz .sum-pthreads
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
