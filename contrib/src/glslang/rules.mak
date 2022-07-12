# glslang

GLSLANG_VERSION := 11.10.0
GLSLANG_URL := https://github.com/KhronosGroup/glslang/archive/refs/tags/$(GLSLANG_VERSION).tar.gz

PKGS += glslang
ifeq ($(call need_pkg,"glslang >= 10"),)
PKGS_FOUND += glslang
endif

$(TARBALLS)/glslang-$(GLSLANG_VERSION).tar.gz:
	$(call download_pkg,$(GLSLANG_URL),glslang)

.sum-glslang: glslang-$(GLSLANG_VERSION).tar.gz

glslang: glslang-$(GLSLANG_VERSION).tar.gz .sum-glslang
	$(UNPACK)
	$(APPLY) $(SRC)/glslang/glslang-win32.patch
	$(MOVE)

.glslang: glslang toolchain.cmake
	cd $< && $(HOSTVARS_PIC) CXXFLAGS="-DYYDEBUG=0" $(CMAKE) -DBUILD_SHARED_LIBS=OFF \
	    -DENABLE_GLSLANG_BINARIES=OFF
	+$(CMAKEBUILD) $< --target install
	touch $@
