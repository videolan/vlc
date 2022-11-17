# glslang

GLSLANG_VERSION := 11.10.0
GLSLANG_URL := $(GITHUB)/KhronosGroup/glslang/archive/refs/tags/$(GLSLANG_VERSION).tar.gz

# glslang doesn't export a pkg-config file, so we check the header manually
GLSLANG_MIN_VER := 10
define GLSLANG_CHECK :=
# include <glslang/build_info.h> \n
# if GLSLANG_VERSION_MAJOR >= $(GLSLANG_MIN_VER) \n
#  define GLSLANG_OK \n
# endif
endef

PKGS += glslang
ifneq ($(call cppcheck, GLSLANG_OK, $(GLSLANG_CHECK)),)
PKGS_FOUND += glslang
endif

$(TARBALLS)/glslang-$(GLSLANG_VERSION).tar.gz:
	$(call download_pkg,$(GLSLANG_URL),glslang)

.sum-glslang: glslang-$(GLSLANG_VERSION).tar.gz

glslang: glslang-$(GLSLANG_VERSION).tar.gz .sum-glslang
	$(UNPACK)
	$(APPLY) $(SRC)/glslang/glslang-win32.patch
	$(MOVE)

GLSLANG_CONF := -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_CTEST=OFF

.glslang: glslang toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) CXXFLAGS="$(CXXFLAGS) -DYYDEBUG=0" $(CMAKE) $(GLSLANG_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
