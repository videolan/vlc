# Kate

KATE_VERSION := 0.4.3
KATE_URL := $(XIPH)/kate/libkate-$(KATE_VERSION).tar.gz

PKGS += kate
ifeq ($(call need_pkg,"kate >= 0.1.5"),)
PKGS_FOUND += kate
endif

$(TARBALLS)/libkate-$(KATE_VERSION).tar.gz:
	$(call download_pkg,$(KATE_URL),kate)

.sum-kate: libkate-$(KATE_VERSION).tar.gz

libkate: libkate-$(KATE_VERSION).tar.gz .sum-kate
	$(UNPACK)
	$(call update_autoconfig,misc/autotools)
	$(MOVE)

DEPS_kate = ogg $(DEPS_ogg)

KATE_CONF := --disable-valgrind --disable-doc

.kate: libkate
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(KATE_CONF)
	+$(MAKEBUILD) SUBDIRS=.
	+$(MAKEBUILD) SUBDIRS=. install
	touch $@
