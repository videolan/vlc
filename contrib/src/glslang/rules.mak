# GLSLANG
GLSLANG_HASH := ef1f899b5d64a9628023f1bb129198674cba2b97
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
	$(APPLY) $(SRC)/glslang/remove-broken-code.patch
	$(MOVE)

.glslang: glslang toolchain.cmake
	cd $< && $(HOSTVARS_PIC) CXXFLAGS="-DYYDEBUG=0" $(CMAKE) -DBUILD_SHARED_LIBS=OFF \
	    -DENABLE_GLSLANG_BINARIES=OFF
	cd $< && $(MAKE) install
	touch $@
