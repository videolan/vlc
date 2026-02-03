# x265

#X265_GITURL := https://github.com/videolan/x265
X265_VERSION := 4.1
X265_SNAPURL := https://bitbucket.org/multicoreware/x265_git/downloads/x265_$(X265_VERSION).tar.gz

ifdef BUILD_ENCODERS
ifdef GPL
PKGS += x265
endif
endif

ifeq ($(call need_pkg,"x265 >= 0.6"),)
PKGS_FOUND += x265
endif

$(TARBALLS)/x265-git.tar.xz:
	$(call download_git,$(X265_GITURL))

$(TARBALLS)/x265_$(X265_VERSION).tar.gz:
	$(call download_pkg,$(X265_SNAPURL),x265)

.sum-x265: x265_$(X265_VERSION).tar.gz

x265: x265_$(X265_VERSION).tar.gz .sum-x265
	$(UNPACK)
	$(APPLY) $(SRC)/x265/0001-fix-ldl-linking-error-of-x265.patch
	$(APPLY) $(SRC)/x265/0003-add-patch-to-enable-detect512.patch
	$(APPLY) $(SRC)/x265/0004-Fix-for-CMake-Build-Errors-in-MacOS.patch
	$(APPLY) $(SRC)/x265/0001-use-OpenFileMappingW-instead-of-OpenFileMappingA.patch
	$(APPLY) $(SRC)/x265/0001-Fix-libunwind-static-linking-on-Android-toolchains.patch
	$(APPLY) $(SRC)/x265/0001-CMake-verify-the-Neon-SVE-compiler-flags-can-be-used.patch
	$(APPLY) $(SRC)/x265/0002-CMake-don-t-force-_WIN32_WINNT-values.patch
	$(APPLY) $(SRC)/x265/0010-CMake-allow-lpthread-in-the-pkg-config-file.patch
	$(APPLY) $(SRC)/x265/0012-CMake-also-use-the-arch-flag-when-cross-compiling-ar.patch
	$(call pkg_static,"source/x265.pc.in")
	$(MOVE)

X265_CONF := -DENABLE_SHARED=OFF -DENABLE_CLI=OFF

ifdef HAVE_DARWIN_OS
ifneq ($(filter arm aarch64, $(ARCH)),)
ifdef HAVE_IOS
X265_CONF += -DASM_FLAGS="-mmacosx-version-min=10.7;-isysroot;$(IOS_SDK)"
else
X265_CONF += -DASM_FLAGS="-mmacosx-version-min=10.7;-isysroot;$(MACOSX_SDK)"
endif
endif
endif

.x265: x265 toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -S $</source $(X265_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
