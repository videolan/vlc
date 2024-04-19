FXC2_HASH := 654c29d62a02714ea0bacfb118c3e05127f846e0
FXC2_VERSION := git-$(FXC2_HASH)
FXC2_GITURL := $(GITHUB)/mozilla/fxc2.git

ifeq ($(call need_pkg,"fxc2"),)
PKGS_FOUND += fxc2
endif

$(TARBALLS)/fxc2-$(FXC2_VERSION).tar.xz:
	$(call download_git,$(FXC2_GITURL),,$(FXC2_HASH))

.sum-fxc2: fxc2-$(FXC2_VERSION).tar.xz
	$(call check_githash,$(FXC2_HASH))
	touch $@

fxc2: fxc2-$(FXC2_VERSION).tar.xz .sum-fxc2
	$(UNPACK)
	$(APPLY) $(SRC)/fxc2/0001-make-Vn-argument-as-optional-and-provide-default-var.patch
	$(APPLY) $(SRC)/fxc2/0002-accept-windows-style-flags-and-splitted-argument-val.patch
	$(APPLY) $(SRC)/fxc2/0004-Revert-Fix-narrowing-conversion-from-int-to-BYTE.patch
	$(APPLY) $(SRC)/fxc2/0001-handle-O-option-to-write-to-a-binary-file-rather-tha.patch
	$(APPLY) $(SRC)/fxc2/0002-fix-redefinition-warning.patch
	$(APPLY) $(SRC)/fxc2/0003-improve-error-messages-after-compilation.patch
	$(MOVE)

ifeq ($(shell uname -m),aarch64)
FXC2_CXX=aarch64-w64-mingw32-g++
FXC2_DLL=dll/d3dcompiler_47_arm64.dll
else ifeq ($(ARCH),x86_64)
FXC2_CXX=$(CXX:uwp-g++=-g++)
FXC2_DLL=dll/d3dcompiler_47.dll
else ifeq ($(ARCH),i386)
FXC2_CXX=$(CXX:uwp-g++=-g++)
FXC2_DLL=dll/d3dcompiler_47_32.dll
else ifeq ($(shell command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 || echo FAIL),)
FXC2_CXX=x86_64-w64-mingw32-g++
FXC2_DLL=dll/d3dcompiler_47.dll
else ifeq ($(shell command -v i686-w64-mingw32-g++ >/dev/null 2>&1 || echo FAIL),)
FXC2_CXX=i686-w64-mingw32-g++
FXC2_DLL=dll/d3dcompiler_47_32.dll
else
FXC2_CXX=$(error No x86 (cross) compiler found for fxc2)
endif


.fxc2: fxc2
	cd $< && $(FXC2_CXX) -static fxc2.cpp -o fxc2.exe
	install -d "$(PREFIX)/bin" && \
        install $</fxc2.exe "$(PREFIX)/bin" && \
        install $</$(FXC2_DLL) "$(PREFIX)/bin/d3dcompiler_47.dll"
	touch $@
