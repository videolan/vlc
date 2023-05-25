# orc

ORC_VERSION := 0.4.18
ORC_URL := $(CONTRIB_VIDEOLAN)/orc/orc-$(ORC_VERSION).tar.gz

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

DEPS_orc :=
ifdef HAVE_WINSTORE
# orc uses VirtualAlloc
DEPS_orc += alloweduwp $(DEPS_alloweduwp)
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.gz:
	$(call download_pkg,$(ORC_URL),orc)

.sum-orc: orc-$(ORC_VERSION).tar.gz

orc: orc-$(ORC_VERSION).tar.gz .sum-orc
	$(UNPACK)
	$(MOVE)

.orc: orc
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
