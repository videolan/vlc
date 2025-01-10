# vpl (Intel Video Processing Library)

VPL_VERSION := 2.13.0
VPL_URL := $(GITHUB)/intel/libvpl/archive/v${VPL_VERSION}/libvpl-${VPL_VERSION}.tar.gz

ifeq ($(call need_pkg,"vpl"),)
PKGS_FOUND += vpl
endif
ifdef HAVE_WIN32
ifndef HAVE_WINSTORE
ifeq ($(filter arm aarch64, $(ARCH)),)
PKGS += vpl
endif
endif
endif

DEPS_vpl :=

ifdef HAVE_WINSTORE
DEPS_vpl += alloweduwp $(DEPS_alloweduwp)
endif

ifdef HAVE_WINSTORE
VPL_VARS := CFLAGS="$(CFLAGS) -DMEDIASDK_UWP_DISPATCHER"
VPL_VARS += CXXFLAGS="$(CXXFLAGS) -DMEDIASDK_UWP_DISPATCHER"
endif

# Disable BUILD_EXPERIMENTAL
# https://github.com/intel/libvpl/issues/168
VPL_CONF := -DBUILD_EXPERIMENTAL=OFF

$(TARBALLS)/libvpl-$(VPL_VERSION).tar.gz:
	$(call download_pkg,$(VPL_URL),vpl)

.sum-vpl: libvpl-$(VPL_VERSION).tar.gz

vpl: libvpl-$(VPL_VERSION).tar.gz .sum-vpl
	$(UNPACK)
	$(APPLY) $(SRC)/vpl/0001-CMake-depend-on-libc-when-compiling-with-Clang.patch
	$(MOVE)

.vpl: vpl toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(VPL_VARS) $(CMAKE) $(VPL_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
