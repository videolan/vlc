# PROJECTM
PROJECTM_VERSION := 2.0.1
PROJECTM_URL := $(SF)/projectm/$(PROJECTM_VERSION)/projectM-$(PROJECTM_VERSION)-Source.tar.gz

ifdef HAVE_WIN32
ifneq ($(ARCH),arm)
ifneq ($(ARCH),aarch64)
ifndef HAVE_WINSTORE
PKGS += projectM
endif
endif
endif
endif
ifeq ($(call need_pkg,"libprojectM"),)
PKGS_FOUND += projectM
endif

DEPS_projectM = glew $(DEPS_glew)

$(TARBALLS)/projectM-$(PROJECTM_VERSION)-Source.tar.gz:
	$(call download_pkg,$(PROJECTM_URL),projectM)

.sum-projectM: projectM-$(PROJECTM_VERSION)-Source.tar.gz

projectM: projectM-$(PROJECTM_VERSION)-Source.tar.gz .sum-projectM
	$(UNPACK)
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/projectM/win64.patch
endif
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/projectM/win32.patch
endif
	$(APPLY) $(SRC)/projectM/gcc6.patch
	$(APPLY) $(SRC)/projectM/clang6.patch
	$(APPLY) $(SRC)/projectM/missing-includes.patch
	$(APPLY) $(SRC)/projectM/projectm-cmake-install.patch
	$(MOVE)

PROJECTM_CONF := \
		-DCMAKE_CXX_STANDARD=98 \
		-DDISABLE_NATIVE_PRESETS:BOOL=ON \
		-DUSE_FTGL:BOOL=OFF \
		-DBUILD_PROJECTM_STATIC:BOOL=ON

.projectM: projectM toolchain.cmake
	rm -f $</build/CMakeCache.txt
	$(HOSTVARS) $(CMAKE) -S $< $(PROJECTM_CONF)
	+$(CMAKEBUILD) $</build --target install
	touch $@
