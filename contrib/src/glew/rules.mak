# GLEW
GLEW_VERSION := 1.5.1
GLEW_URL := $(SF)/glew/glew/$(GLEW_VERSION)/glew-$(GLEW_VERSION)-src.tgz

$(TARBALLS)/glew-$(GLEW_VERSION)-src.tar.gz:
	$(call download,$(GLEW_URL))

.sum-glew: glew-$(GLEW_VERSION)-src.tar.gz

glew: glew-$(GLEW_VERSION)-src.tar.gz .sum-glew
	$(UNPACK)
	mv glew glew-$(GLEW_VERSION)-src
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/glew/win32.patch
endif
	$(MOVE)

.glew: glew
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DGLEW_STATIC" $(MAKE)
	cd $< && $(HOSTVARS) GLEW_DEST=$(PREFIX) $(MAKE) install
	touch $@
