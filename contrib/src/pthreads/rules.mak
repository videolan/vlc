# POSIX threads

ifndef HAVE_WIN32
PKGS_FOUND += pthreads
endif

PTHREADS_W32_VERSION := 2-9-1
PTHREADS_W32_URL := ftp://sources.redhat.com/pub/pthreads-win32/pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz

$(TARBALLS)/pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz:
	$(call download_pkg,$(PTHREADS_W32_URL),pthreads)

.sum-pthreads: pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz

ifdef HAVE_WIN32
pthreads: pthreads-w32-$(PTHREADS_W32_VERSION)-release.tar.gz .sum-pthreads
	$(UNPACK)
	sed -e 's/^CROSS.*=/CROSS ?=/' -i.orig $(UNPACK_DIR)/GNUmakefile
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/pthreads/winrt.patch
endif
	$(APPLY) $(SRC)/pthreads/implib.patch
	$(APPLY) $(SRC)/pthreads/remove-inline.patch
	$(APPLY) $(SRC)/pthreads/x86-inline-asm.patch
	$(APPLY) $(SRC)/pthreads/arm64.patch
	$(APPLY) $(SRC)/pthreads/pthreads-fix-mode_t.patch
	$(APPLY) $(SRC)/pthreads/pthread-fix-inline.patch
	$(MOVE)

PTHREADS_W32_CONF := LFLAGS="$(LDFLAGS)" PTW32_FLAGS="$(CFLAGS)"
ifdef HAVE_CROSS_COMPILE
PTHREADS_W32_CONF += CROSS="$(HOST)-"
endif

.pthreads: pthreads
	cd $< && $(HOSTVARS) $(PTHREADS_W32_CONF) $(MAKE) MAKEFLAGS=-j1 GC GC-static
	mkdir -p -- "$(PREFIX)/include"
	cd $< && cp -v pthread.h sched.h semaphore.h "$(PREFIX)/include/"
	sed -e 's/#if HAVE_CONFIG_H/#if 0 \&\& HAVE_CONFIG_H/' -i \
		"$(PREFIX)/include/pthread.h"
	mkdir -p -- "$(PREFIX)/lib"
	cp -v $</*.a $</*.dll "$(PREFIX)/lib/"
	cp -f "$(PREFIX)/lib/libpthreadGC2.a" "$(PREFIX)/lib/libpthread.a"
	touch $@
endif
