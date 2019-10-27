# GLEW
GLEW_VERSION := 2.1.0
GLEW_URL := $(SF)/glew/glew/$(GLEW_VERSION)/glew-$(GLEW_VERSION).tgz

ifeq ($(call need_pkg,"glew"),)
PKGS_FOUND += glew
endif

$(TARBALLS)/glew-$(GLEW_VERSION).tgz:
	$(call download_pkg,$(GLEW_URL),glew)

.sum-glew: glew-$(GLEW_VERSION).tgz

glew: glew-$(GLEW_VERSION).tgz .sum-glew
	$(UNPACK)
	$(APPLY) $(SRC)/glew/glew-drop-debug-postfix.patch
	$(MOVE)

.glew: glew toolchain.cmake
	cd $</build/cmake && $(HOSTVARS_PIC) $(CMAKE) -DBUILD_SHARED_LIBS:BOOL=OFF -DGLEW_USE_STATIC_LIBS:BOOL=ON
	cd $</build/cmake && $(MAKE) install
	touch $@
