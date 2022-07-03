# GLSLANG
GLSLANG_HASH := adbf0d3106b26daa237b10b9bf72b1af7c31092d
GLSLANG_BRANCH := master
GLSLANG_GITURL := https://github.com/KhronosGroup/glslang.git
GLSLANG_BASENAME := $(subst .,_,$(subst \,_,$(subst /,_,$(GLSLANG_HASH))))

PKGS += glslang
ifeq ($(call need_pkg,"glslang"),)
PKGS_FOUND += glslang
endif

$(TARBALLS)/glslang-$(GLSLANG_BASENAME).tar.xz:
	$(call download_git,$(GLSLANG_GITURL),$(GLSLANG_BRANCH),$(GLSLANG_HASH))

.sum-glslang: $(TARBALLS)/glslang-$(GLSLANG_BASENAME).tar.xz
	$(call check_githash,$(GLSLANG_HASH))
	touch $@

glslang: glslang-$(GLSLANG_BASENAME).tar.xz .sum-glslang
	$(UNPACK)
	$(APPLY) $(SRC)/glslang/glslang-win32.patch
	$(MOVE)

.glslang: glslang toolchain.cmake
	cd $< && $(HOSTVARS_PIC) CXXFLAGS="-DYYDEBUG=0" $(CMAKE) -DBUILD_SHARED_LIBS=OFF \
	    -DENABLE_GLSLANG_BINARIES=OFF
	+$(CMAKEBUILD) $< --target install
	touch $@
