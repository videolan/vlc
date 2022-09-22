# Kate

KATE_VERSION := 0.4.1
KATE_URL := $(GOOGLE_CODE)/libkate/libkate-$(KATE_VERSION).tar.gz

PKGS += kate
ifeq ($(call need_pkg,"kate >= 0.1.5"),)
PKGS_FOUND += kate
endif

$(TARBALLS)/libkate-$(KATE_VERSION).tar.gz:
	$(call download_pkg,$(KATE_URL),kate)

.sum-kate: libkate-$(KATE_VERSION).tar.gz

libkate: libkate-$(KATE_VERSION).tar.gz .sum-kate
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv libkate/config.sub libkate/config.guess libkate/misc/autotools

DEPS_kate = ogg $(DEPS_ogg)

KATE_CONF := --disable-valgrind --disable-doc

.kate: libkate
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(KATE_CONF)
	+$(MAKEBUILD) SUBDIRS=.
	+$(MAKEBUILD) SUBDIRS=. install
	touch $@
