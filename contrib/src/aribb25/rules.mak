# aribb25

ARIBB25_VERSION := 0.2.7
ARIBB25_URL := $(VIDEOLAN)/aribb25/$(ARIBB25_VERSION)/aribb25-$(ARIBB25_VERSION).tar.gz

ifdef HAVE_WIN32
PKGS += aribb25
endif
ifeq ($(call need_pkg,"pcslite"),)
PKGS += aribb25
endif
ifeq ($(call need_pkg,"aribb25"),)
PKGS_FOUND += aribb25
endif

$(TARBALLS)/aribb25-$(ARIBB25_VERSION).tar.gz:
	$(call download_pkg,$(ARIBB25_URL),aribb25)

.sum-aribb25: aribb25-$(ARIBB25_VERSION).tar.gz

aribb25: aribb25-$(ARIBB25_VERSION).tar.gz .sum-aribb25
	$(UNPACK)
	$(MOVE)

.aribb25: aribb25
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
