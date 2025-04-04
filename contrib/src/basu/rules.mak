# basu
BASU_VERSION := 0.2.1
BASU_URL := https://git.sr.ht/~emersion/basu/refs/download/v$(BASU_VERSION)/basu-$(BASU_VERSION).tar.gz

ifneq ($(call need_pkg,"libelogind"),)
ifneq ($(call need_pkg,"libsystemd"),)


ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += basu
endif
endif

ifdef HAVE_BSD
PKGS += basu
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
	$(MESON)
	+$(MESONBUILD)
	touch $@
