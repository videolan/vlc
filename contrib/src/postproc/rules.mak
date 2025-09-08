# libpostproc

POSTPROC_GITURL:=$(GITHUB)/michaelni/libpostproc
POSTPROC_GITVERSION:=e4982b227779f24f7c54d699d2e247652f69c475

POSTPROC_CONF = --prefix="$(PREFIX)" --enable-static --disable-shared \
	--extra-ldflags="$(LDFLAGS)" \
	--cc="$(CC)" \
	--host-cc="$(BUILDCC)" \
	--pkg-config="$(PKG_CONFIG)" \
	--disable-doc \
	--disable-debug \
	--disable-devices \
	--disable-bsfs \
	--disable-nvenc \
	--disable-linux-perf

ifdef ENABLE_PDB
POSTPROC_CONF += --ln_s=false
endif

DEPS_postproc = ffmpeg $(DEPS_ffmpeg)

POSTPROC_CONF += --disable-network
POSTPROC_CONF += --disable-encoders --disable-muxers

# Small size
ifdef WITH_OPTIMIZATION
ifdef ENABLE_SMALL
POSTPROC_CONF += --enable-small
endif
ifeq ($(ARCH),arm)
ifdef HAVE_ARMV7A
POSTPROC_CONF += --enable-thumb
endif
endif
else
POSTPROC_CONF += --optflags=-Og
endif

ifdef HAVE_CROSS_COMPILE
POSTPROC_CONF += --enable-cross-compile --disable-programs
ifndef HAVE_DARWIN_OS
POSTPROC_CONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
POSTPROC_CONF += --arch=arm
ifdef HAVE_ARMV7A
POSTPROC_CONF += --cpu=cortex-a8
endif
ifdef HAVE_ARMV6
POSTPROC_CONF += --cpu=armv6 --disable-neon
endif
endif

# ARM64 stuff
ifeq ($(ARCH),aarch64)
POSTPROC_CONF += --arch=aarch64
endif

# MIPS stuff
ifeq ($(ARCH),mipsel)
POSTPROC_CONF += --arch=mips
endif
ifeq ($(ARCH),mips64el)
POSTPROC_CONF += --arch=mips64
endif

# RISC-V stuff
ifneq ($(findstring $(ARCH),riscv32 riscv64),)
POSTPROC_CONF += --arch=riscv
endif

# x86 stuff
ifeq ($(ARCH),i386)
ifndef HAVE_DARWIN_OS
POSTPROC_CONF += --arch=x86
endif
endif

# x86_64 stuff
ifeq ($(ARCH),x86_64)
ifndef HAVE_DARWIN_OS
POSTPROC_CONF += --arch=x86_64
endif
endif

# Darwin
ifdef HAVE_DARWIN_OS
ifeq ($(ARCH),arm64_32)
# TODO remove when FFMpeg supports arm64_32
POSTPROC_CONF += --arch=aarch64_32
else
POSTPROC_CONF += --arch=$(ARCH)
endif
POSTPROC_CONF += --target-os=darwin --extra-cflags="$(CFLAGS)"
ifeq ($(ARCH),x86_64)
POSTPROC_CONF += --cpu=core2
endif
ifdef HAVE_IOS
POSTPROC_CONF += --enable-pic --extra-ldflags="$(EXTRA_CFLAGS) -isysroot $(IOS_SDK)"
ifdef HAVE_WATCHOS
POSTPROC_CONF += --disable-everything
endif
endif
endif

# Linux
ifdef HAVE_LINUX
POSTPROC_CONF += --target-os=linux --enable-pic

endif

ifdef HAVE_ANDROID
# broken text relocations
ifeq ($(ANDROID_ABI), x86)
POSTPROC_CONF +=  --disable-mmx --disable-mmxext --disable-inline-asm
endif
endif

POSTPROC_CONF += --disable-autodetect
# use the real maintained libavutil, only headers should be used
# DO NOT install this libavutil in place of the FFmpeg one
POSTPROC_CONF += --disable-avutil
POSTPROC_CONF += --enable-postproc

# Windows
ifdef HAVE_WIN32
POSTPROC_CONF += --target-os=mingw32
POSTPROC_CONF += --enable-w32threads

ifeq ($(ARCH),x86_64)
POSTPROC_CONF += --cpu=athlon64 --arch=x86_64
else
ifeq ($(ARCH),i386) # 32bits intel
POSTPROC_CONF+= --cpu=i686 --arch=x86
else
ifdef HAVE_ARMV7A
POSTPROC_CONF+= --arch=arm
endif
endif
endif

else # !Windows
POSTPROC_CONF += --enable-pthreads
endif

# Solaris
ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
POSTPROC_CONF += --cpu=core2
endif
POSTPROC_CONF += --target-os=sunos --enable-pic
endif

ifdef HAVE_EMSCRIPTEN
POSTPROC_CONF+= --arch=wasm32 --target-os=none --enable-pic
endif

# Build
ifdef GPL
PKGS += postproc
endif
ifeq ($(call need_pkg,"libpostproc"),)
PKGS_FOUND += postproc
endif

POSTPROC_CONF += --nm="$(NM)" --ar="$(AR)" --ranlib="$(RANLIB)"

$(TARBALLS)/postproc-$(POSTPROC_GITVERSION).tar.xz:
	$(call download_git,$(POSTPROC_GITURL),,$(POSTPROC_GITVERSION))

.sum-postproc: $(TARBALLS)/postproc-$(POSTPROC_GITVERSION).tar.xz
	$(call check_githash,$(POSTPROC_GITVERSION))
	touch $@

postproc: postproc-$(POSTPROC_GITVERSION).tar.xz .sum-postproc
	$(UNPACK)
	$(APPLY) $(SRC)/postproc/0001-force-using-external-libavutil.patch
	$(APPLY) $(SRC)/postproc/0002-add-missing-libavcodec-headers.patch
	$(MOVE)

.postproc: postproc
	$(REQUIRE_GPL)
	$(MAKEBUILDDIR)
	$(MAKECONFDIR)/configure $(POSTPROC_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install-libs install-headers
	touch $@
