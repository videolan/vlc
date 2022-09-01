# aom
AOM_VERSION := 3.3.0
AOM_URL := https://storage.googleapis.com/aom-releases/libaom-$(AOM_VERSION).tar.gz

PKGS += aom
ifeq ($(call need_pkg,"aom"),)
PKGS_FOUND += aom
endif

$(TARBALLS)/libaom-$(AOM_VERSION).tar.gz:
	$(call download_pkg,$(AOM_URL),aom)

.sum-aom: libaom-$(AOM_VERSION).tar.gz

aom: libaom-$(AOM_VERSION).tar.gz .sum-aom
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/aom/aom-android-pthreads.patch
	$(APPLY) $(SRC)/aom/aom-android-cpufeatures.patch
endif
	$(MOVE)
ifdef HAVE_ANDROID
	cp $(ANDROID_NDK)/sources/android/cpufeatures/cpu-features.c $(ANDROID_NDK)/sources/android/cpufeatures/cpu-features.h aom/aom_ports/
endif

DEPS_aom =
ifdef HAVE_WIN32
DEPS_aom += pthreads $(DEPS_pthreads)
endif

AOM_CONF := \
	-DCONFIG_RUNTIME_CPU_DETECT=1 \
	-DCONFIG_MULTITHREAD=1 \
	-DENABLE_DOCS=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TOOLS=OFF \
	-DENABLE_TESTS=OFF

ifndef BUILD_ENCODERS
AOM_CONF += -DCONFIG_AV1_ENCODER=0
endif

ifndef HAVE_WIN32
AOM_CONF += -DCONFIG_PIC=1
endif

ifdef HAVE_WIN32
ifneq ($(filter arm aarch64, $(ARCH)),)
# These targets don't have runtime cpu detection.
AOM_CONF += -DCONFIG_RUNTIME_CPU_DETECT=0
endif
ifeq ($(ARCH),arm)
# armv7, not just plain arm
AOM_CONF += -DAOM_ADS2GAS_REQUIRED=1 -DAOM_ADS2GAS=../build/make/ads2gas.pl -DAOM_ADS2GAS_OPTS="-thumb;-noelf" -DAOM_GAS_EXT=S
endif
endif

ifdef HAVE_DARWIN_OS
ifneq ($(filter arm aarch64, $(ARCH)),)
# These targets don't have runtime cpu detection.
AOM_CONF += -DCONFIG_RUNTIME_CPU_DETECT=0
endif
endif

# Force cpu detection
ifdef HAVE_ANDROID
ifeq ($(ARCH),aarch64)
AOM_CONF += -DAOM_TARGET_CPU=arm64
endif
endif

ifeq ($(ARCH),arm)
# armv7, not just plain arm
AOM_CONF += -DAOM_TARGET_CPU=armv7
endif

# libaom doesn't allow in-tree builds
.aom: aom toolchain.cmake
	rm -rf $(PREFIX)/include/aom
	cd $< && rm -rf aom_build && mkdir -p aom_build
	cd $</aom_build && $(HOSTVARS) $(CMAKE) ../ $(AOM_CONF)
	+$(CMAKEBUILD) $</aom_build
	$(call pkg_static,"aom_build/aom.pc")
	+$(CMAKEBUILD) $</aom_build --target install
	touch $@
