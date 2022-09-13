# basu
BASU_VERSION := 0.2.0
BASU_URL := http://git.sr.ht/~emersion/basu/refs/download/v$(BASU_VERSION)/basu-$(BASU_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += basu
endif
endif

ifdef HAVE_BSD
ifndef HAVE_DARWIN_OS
PKGS += basu
endif
endif

$(TARBALLS)/basu-$(BASU_VERSION).tar.gz:
	$(call download_pkg,$(BASU_URL),basu)

.sum-basu: basu-$(BASU_VERSION).tar.gz

basu: basu-$(BASU_VERSION).tar.gz .sum-basu
	$(UNPACK)
	$(MOVE)

.basu: basu crossfile.meson
	rm -rf $</build
	$(HOSTVARS_MESON) $(MESON) $</build $<
	cd $< && cd build && ninja install
	touch $@
