# basu
BASU_VERSION := 0.2.0
BASU_URL := http://git.sr.ht/~emersion/basu/refs/download/v$(BASU_VERSION)/basu-$(BASU_VERSION).tar.gz

ifneq ($(call need_pkg,"libelogind"),)
ifneq ($(call need_pkg,"libsystemd"),)


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

endif # libsystemd
endif # libelogind

ifeq ($(call need_pkg,"basu"),)
PKGS_FOUND += basu
endif

$(TARBALLS)/basu-$(BASU_VERSION).tar.gz:
	$(call download_pkg,$(BASU_URL),basu)

.sum-basu: basu-$(BASU_VERSION).tar.gz

basu: basu-$(BASU_VERSION).tar.gz .sum-basu
	$(UNPACK)
	$(MOVE)

.basu: basu crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON)
	$(MESONBUILD)
	touch $@
