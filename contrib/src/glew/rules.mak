# GLEW
GLEW_VERSION := 2.3.1
GLEW_URL := $(GITHUB)/nigels-com/glew/releases/download/glew-$(GLEW_VERSION)/glew-$(GLEW_VERSION).tgz

ifeq ($(call need_pkg,"glew"),)
PKGS_FOUND += glew
endif

$(TARBALLS)/glew-$(GLEW_VERSION).tgz:
	$(call download_pkg,$(GLEW_URL),glew)

.sum-glew: glew-$(GLEW_VERSION).tgz

glew: glew-$(GLEW_VERSION).tgz .sum-glew
	$(UNPACK)
	$(APPLY) $(SRC)/glew/0001-Define-GLEW_STATIC-in-pkg-config-file-when-compiled-.patch
	$(APPLY) $(SRC)/glew/0002-Link-with-opengl32-on-Windows.patch
	$(APPLY) $(SRC)/glew/0003-Link-directly-with-glu32-on-Windows.patch
	$(APPLY) $(SRC)/glew/0004-Allow-disabling-the-CMAKE_DEBUG_POSTFIX.patch
	$(MOVE)

GLEW_CONF := -DBUILD_UTILS=OFF

.glew: glew toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -S $</build/cmake $(GLEW_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
