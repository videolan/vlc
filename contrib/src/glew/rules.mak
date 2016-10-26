# GLEW
GLEW_VERSION := 1.7.0
GLEW_URL := $(SF)/glew/glew/$(GLEW_VERSION)/glew-$(GLEW_VERSION).tgz

ifeq ($(call need_pkg,"glew"),)
PKGS_FOUND += glew
endif

$(TARBALLS)/glew-$(GLEW_VERSION).tar.gz:
	$(call download_pkg,$(GLEW_URL),glew)

.sum-glew: glew-$(GLEW_VERSION).tar.gz

glew: glew-$(GLEW_VERSION).tar.gz .sum-glew
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/glew/win32.patch
endif
	$(MOVE)

.glew: glew
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DGLEW_STATIC" $(MAKE)
	cd $< && $(HOSTVARS) GLEW_DEST=$(PREFIX) $(MAKE) install
ifdef HAVE_WIN32
	-rm $(PREFIX)/lib/*glew32.dll*
endif
	touch $@
