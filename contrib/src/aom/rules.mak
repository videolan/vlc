# aom
AOM_HASH := add4b15580e410c00c927ee366fa65545045a5d9
AOM_VERSION := v1.0.0.errata.1
AOM_GITURL := https://aomedia.googlesource.com/aom/+archive/$(AOM_HASH).tar.gz

PKGS += aom
ifeq ($(call need_pkg,"aom"),)
PKGS_FOUND += aom
endif

$(TARBALLS)/aom-$(AOM_VERSION).tar.gz:
	$(call download,$(AOM_GITURL))

.sum-aom: aom-$(AOM_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

aom: aom-$(AOM_VERSION).tar.gz .sum-aom
	rm -Rf $@-$(AOM_VERSION) $@
	mkdir -p $@-$(AOM_VERSION)
	tar xvzfo "$<" -C $@-$(AOM_VERSION)
	$(APPLY) $(SRC)/aom/aom-target-cpu.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/aom/aom-android-pthreads.patch
	$(APPLY) $(SRC)/aom/aom-android-cpufeatures.patch
endif
	$(MOVE)
ifdef HAVE_ANDROID
	cp $(ANDROID_NDK)/sources/android/cpufeatures/cpu-features.c $(ANDROID_NDK)/sources/android/cpufeatures/cpu-features.h aom/aom_ports/
endif

DEPS_aom =

AOM_LDFLAGS := $(LDFLAGS)

AOM_CONF := \
	-DCONFIG_RUNTIME_CPU_DETECT=1 \
	-DCONFIG_MULTITHREAD=1 \
	-DENABLE_DOCS=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TOOLS=OFF \
	-DCONFIG_UNIT_TESTS=0 \
	-DENABLE_TESTS=OFF \
	-DCONFIG_INSTALL_BINS=0 \
	-DCONFIG_INSTALL_DOCS=0 \
	-DCONFIG_DEPENDENCY_TRACKING=0 \
	-DCONFIG_AV1_ENCODER=0

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
AOM_CONF += -DAOM_TARGET_CPU=armv7
AOM_CONF += -DAOM_ADS2GAS_REQUIRED=1 -DAOM_ADS2GAS=../build/make/ads2gas.pl -DAOM_ADS2GAS_OPTS="-thumb;-noelf" -DAOM_GAS_EXT=S
endif
endif

ifdef HAVE_IOS
ifneq ($(filter arm aarch64, $(ARCH)),)
# These targets don't have runtime cpu detection.
AOM_CONF += -DCONFIG_RUNTIME_CPU_DETECT=0
endif
ifeq ($(ARCH),arm)
# armv7, not just plain arm
AOM_CONF += -DAOM_TARGET_CPU=armv7
endif
endif

# Force cpu detection
ifdef HAVE_ANDROID
ifeq ($(ARCH),arm)
AOM_CONF += -DAOM_TARGET_CPU=armv7
endif
ifeq ($(ARCH),aarch64)
AOM_CONF += -DAOM_TARGET_CPU=arm64
endif
endif

# libaom doesn't allow in-tree builds
.aom: aom toolchain.cmake
	cd $< && mkdir -p aom_build
	cd $</aom_build && LDFLAGS="$(AOM_LDFLAGS)" $(HOSTVARS) $(CMAKE) ../ $(AOM_CONF)
	cd $</aom_build && $(MAKE)
	cd $</aom_build && ../../../../contrib/src/pkg-static.sh aom.pc
	cd $</aom_build && $(MAKE) install
	touch $@
