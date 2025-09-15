# libvpx

VPX_VERSION := 1.15.2
VPX_URL := $(GITHUB)/webmproject/libvpx/archive/v${VPX_VERSION}.tar.gz

ifneq ($(filter arm aarch64 i386 loongarch64 mipsel mips64el ppc64le x86_64 wasm32, $(ARCH)),)
PKGS += vpx
endif
ifeq ($(call need_pkg,"vpx >= 1.5.0"),)
PKGS_FOUND += vpx
endif

$(TARBALLS)/libvpx-$(VPX_VERSION).tar.gz:
	$(call download_pkg,$(VPX_URL),vpx)

.sum-vpx: libvpx-$(VPX_VERSION).tar.gz

libvpx: libvpx-$(VPX_VERSION).tar.gz .sum-vpx
	$(UNPACK)
	$(APPLY) $(SRC)/vpx/libvpx-ios.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/vpx/libvpx-android.patch
	cp "${ANDROID_NDK}"/sources/android/cpufeatures/cpu-features.c $(UNPACK_DIR)/vpx_ports
	cp "${ANDROID_NDK}"/sources/android/cpufeatures/cpu-features.h $(UNPACK_DIR)
endif
ifdef HAVE_MACOSX
ifeq ($(ARCH),aarch64)
	$(APPLY) $(SRC)/vpx/libvpx-darwin-aarch64.patch
endif
endif
	# Disable automatic addition of -fembed-bitcode for iOS
	# as it is enabled through --extra-cflags if necessary.
	$(APPLY) $(SRC)/vpx/libvpx-remove-bitcode.patch
	$(MOVE)

DEPS_vpx =

ifdef HAVE_WIN32
DEPS_vpx += winpthreads $(DEPS_winpthreads)
endif

ifdef HAVE_CROSS_COMPILE
VPX_CROSS := $(HOST)-
else
VPX_CROSS :=
endif

VPX_LDFLAGS := $(LDFLAGS)

ifeq ($(ARCH),arm)
VPX_ARCH := armv7
else ifeq ($(ARCH),i386)
VPX_ARCH := x86
else ifeq ($(ARCH),mips)
VPX_ARCH := mips32
else ifeq ($(ARCH),ppc)
VPX_ARCH := ppc32
else ifeq ($(ARCH),aarch64)
VPX_ARCH := arm64
else
VPX_ARCH := $(ARCH)
endif

ifdef HAVE_ANDROID
VPX_OS := android
else ifdef HAVE_LINUX
VPX_OS := linux
else ifdef HAVE_DARWIN_OS
VPX_CROSS :=
ifeq ($(ARCH),$(filter $(ARCH), arm aarch64))
VPX_OS := darwin
else
VPX_OS := darwin11
endif
else ifdef HAVE_SOLARIS
VPX_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
VPX_OS := win64
else ifdef HAVE_WIN32
VPX_OS := win32
else ifdef HAVE_BSD
VPX_OS := linux
endif

VPX_TARGET := generic-gnu
ifdef VPX_ARCH
ifdef VPX_OS
VPX_TARGET := $(VPX_ARCH)-$(VPX_OS)-gcc
endif
endif

VPX_CONF := \
	--disable-docs \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--disable-dependency-tracking \
	--enable-vp9-highbitdepth \
	--disable-tools \
	--target=$(VPX_TARGET) \
	--prefix=$(PREFIX)

ifneq ($(filter arm aarch64, $(ARCH)),)
# Only enable runtime cpu detect on architectures other than arm/aarch64
# when building for Windows and Darwin
ifndef HAVE_WIN32
ifndef HAVE_DARWIN_OS
VPX_CONF += --enable-runtime-cpu-detect
endif
endif
endif

VPX_CFLAGS=$(CFLAGS)

ifndef BUILD_ENCODERS
VPX_CONF += --disable-vp8-encoder --disable-vp9-encoder
endif

ifeq ($(ARCH),i386)
VPX_CFLAGS += -mstackrealign
endif

ifndef HAVE_WIN32
VPX_CONF += --enable-pic
else
ifeq ($(ARCH),arm)
# As of libvpx 1.14.0 we have to explicitly disable runtime CPU detection for Windows armv7
VPX_CONF += --disable-runtime-cpu-detect
endif
endif
ifdef HAVE_IOS
ifeq ($(ARCH),arm)
# As of libvpx 1.14.0 we have to explicitly disable runtime CPU detection for iOS arm7
VPX_CONF += --disable-runtime-cpu-detect
endif
VPX_CONF += --enable-vp8-decoder
VPX_LDFLAGS := -L$(IOS_SDK)/usr/lib -isysroot $(IOS_SDK) $(LDFLAGS)
ifeq ($(ARCH),aarch64)
VPX_LDFLAGS += -arch arm64
endif
endif
ifdef HAVE_DARWIN_OS
ifeq ($(ARCH),$(filter $(ARCH), arm aarch64))
ifneq ($(call clang_at_least, 13), true)
# arm_neon.h broken on clang 12
VPX_CONF += --disable-neon-dotprod
endif
endif
endif

ifndef WITH_OPTIMIZATION
VPX_CONF += --disable-optimizations
endif

# Always enable debug symbols, we strip in the final executables if needed
VPX_CONF += --enable-debug

ifdef HAVE_ANDROID
# Starting NDK19, standalone toolchains are deprecated and gcc is not shipped.
# The presence of gcc can be used to detect if we are using an old standalone
# toolchain. Unfortunately, libvpx buildsystem only work with standalone
# toolchains, therefore pass the HOSTVARS directly to bypass any detection.
ifneq ($(shell $(VPX_CROSS)gcc -v >/dev/null 2>&1 || echo FAIL),)
VPX_HOSTVARS = $(HOSTVARS)
endif

# Depends on "arm-linux-androideabi-as" that is removed in NDK25
ifeq ($(ARCH),arm)
VPX_CONF += --disable-neon_asm
endif
endif

VPX_CONF += --extra-cflags="$(VPX_CFLAGS)"

.vpx: libvpx
	rm -rf $(PREFIX)/include/vpx
	$(MAKEBUILDDIR)
	cd $(BUILD_DIR) && LDFLAGS="$(VPX_LDFLAGS)" CROSS=$(VPX_CROSS) $(VPX_HOSTVARS) $(BUILD_SRC)/configure $(VPX_CONF)
	+CONFIG_DEBUG=1 $(MAKEBUILD)
	$(call pkg_static,"$(BUILD_DIRUNPACK)/vpx.pc")
	+CONFIG_DEBUG=1 $(MAKEBUILD) install
	touch $@
