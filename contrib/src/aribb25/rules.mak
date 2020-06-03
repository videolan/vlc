# aribb25

ARIBB25_VERSION := 0.2.7
ARIBB25_URL := $(VIDEOLAN)/aribb25/$(ARIBB25_VERSION)/aribb25-$(ARIBB25_VERSION).tar.gz

ifdef HAVE_WIN32
ifndef HAVE_WINSTORE
PKGS += aribb25
endif
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
	$(APPLY) $(SRC)/aribb25/0001-fix-build-script.patch
	$(APPLY) $(SRC)/aribb25/0002-fix-libs-include.patch
	$(APPLY) $(SRC)/aribb25/0001-add-an-option-not-to-build-the-b25-sample-code.patch
	$(MOVE)

.aribb25: aribb25
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-b25
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
