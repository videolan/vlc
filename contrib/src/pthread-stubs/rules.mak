PTHREAD_STUBS_VERSION := 0.4
PTHREAD_STUBS_URL := https://xcb.freedesktop.org/dist/libpthread-stubs-$(PTHREAD_STUBS_VERSION).tar.bz2

$(TARBALLS)/libpthread-stubs-$(PTHREAD_STUBS_VERSION).tar.bz2:
	$(call download_pkg,$(PTHREAD_STUBS_URL),pthreads)

.sum-pthread-stubs: libpthread-stubs-$(PTHREAD_STUBS_VERSION).tar.bz2

libpthread-stubs: libpthread-stubs-$(PTHREAD_STUBS_VERSION).tar.bz2 .sum-pthread-stubs
	$(UNPACK)
	$(call pkg_static,"pthread-stubs.pc.in")
	$(MOVE)

.pthread-stubs: libpthread-stubs
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
