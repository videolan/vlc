# PROJECTM
PROJECTM_VERSION := 4.1.6
PROJECTM_URL := $(GITHUB)/projectM-visualizer/projectm/releases/download/v$(PROJECTM_VERSION)/libprojectM-$(PROJECTM_VERSION).tar.gz

ifdef HAVE_WIN32
ifneq ($(ARCH),arm)
ifneq ($(ARCH),aarch64)
ifndef HAVE_WINSTORE # no OpenGL
PKGS += projectM
endif
endif
endif
endif
ifeq ($(call need_pkg,"projectM-4"),)
PKGS_FOUND += projectM
endif

DEPS_projectM = glew $(DEPS_glew)

$(TARBALLS)/libprojectM-$(PROJECTM_VERSION).tar.gz:
	$(call download_pkg,$(PROJECTM_URL),projectM)

.sum-projectM: libprojectM-$(PROJECTM_VERSION).tar.gz

projectM: libprojectM-$(PROJECTM_VERSION).tar.gz .sum-projectM
	$(UNPACK)
	$(APPLY) $(SRC)/projectM/0001-Always-generate-pkg-config-files.patch
	$(APPLY) $(SRC)/projectM/0002-Install-the-regular-projectM-4.pc-for-debug-builds.patch
	$(APPLY) $(SRC)/projectM/0003-Require-glew-package-when-building-for-Windows.patch
	$(APPLY) $(SRC)/projectM/0004-Do-not-use-the-l-form-of-link-flags.patch
	$(MOVE)

PROJECTM_CONF := \
		-DENABLE_DEBUG_POSTFIX:BOOL=OFF \
		-DENABLE_PLAYLIST:BOOL=OFF

.projectM: projectM toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(PROJECTM_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
