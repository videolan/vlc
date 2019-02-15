# POSTPROC

POSTPROC_URL := http://git.videolan.org/git/libpostproc.git
POSTPROC_HASH := 3b7053f46dbfe4662063345245cb00b6acbbe969
POSTPROC_VERSION := $(POSTPROC_HASH)

POSTPROCCONF = \
	--cc="$(CC)" \
	--ar="$(AR)" \
	--ranlib="$(RANLIB)" \
	--disable-debug \
	--enable-gpl \
	--enable-postproc

DEPS_postproc = ffmpeg

ifdef ENABLE_SMALL
POSTPROCCONF += --enable-small
endif
ifdef HAVE_ARMV7A
POSTPROCCONF += --enable-thumb
endif

ifdef HAVE_CROSS_COMPILE
POSTPROCCONF += --enable-cross-compile
ifndef HAVE_IOS
POSTPROCCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
POSTPROCCONF += --disable-runtime-cpudetect --arch=arm
ifdef HAVE_ARMV7A
POSTPROCCONF += --cpu=cortex-a8
endif
ifdef HAVE_NEON
POSTPROCCONF += --enable-neon
endif
endif

# ARM64 stuff
ifeq ($(ARCH),aarch64)
POSTPROCCONF += --arch=aarch64
endif

# MIPS stuff
ifeq ($(ARCH),mipsel)
POSTPROCCONF += --arch=mips
endif
ifeq ($(ARCH),mips64el)
POSTPROCCONF += --arch=mips64
endif

# x86 stuff
ifeq ($(ARCH),i386)
POSTPROCCONF += --arch=x86
endif

# x86_64 stuff
ifeq ($(ARCH),x86_64)
POSTPROCCONF += --arch=x64_64
endif

# Darwin
ifdef HAVE_DARWIN_OS
POSTPROCCONF += --arch=$(ARCH) --target-os=darwin
endif
ifeq ($(ARCH),x86_64)
POSTPROCCONF += --cpu=core2
endif
ifdef HAVE_IOS
ifeq ($(ARCH),arm)
POSTPROCCONF += --as="$(AS)"
endif
endif

# Linux
ifdef HAVE_LINUX
POSTPROCCONF += --target-os=linux --enable-pic
endif

ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
POSTPROCCONF +=  --disable-mmx --disable-mmxext
endif
endif

# Windows
ifdef HAVE_WIN32
POSTPROCCONF += --target-os=mingw32
ifeq ($(ARCH),x86_64)
POSTPROCCONF += --cpu=athlon64 --arch=x86_64
endif
ifeq ($(ARCH),i386)
POSTPROCCONF+= --cpu=i686 --arch=x86
endif
else
POSTPROCCONF += --enable-pthreads
endif

ifdef HAVE_SOLARIS
POSTPROCCONF += --enable-pic
endif

ifdef HAVE_NACL
POSTPROCCONF += --target-os=linux
endif

# Build

ifdef GPL
PKGS += postproc
endif
ifeq ($(call need_pkg,"libpostproc"),)
PKGS_FOUND += postproc
endif

$(TARBALLS)/postproc-$(POSTPROC_VERSION).tar.xz:
	$(call download_git,$(POSTPROC_URL),,$(POSTPROC_HASH))

.sum-postproc: $(TARBALLS)/postproc-$(POSTPROC_VERSION).tar.xz
	$(call check_githash,$(POSTPROC_HASH))
	touch $@

postproc: postproc-$(POSTPROC_VERSION).tar.xz .sum-postproc
	$(UNPACK)
	$(APPLY) $(SRC)/postproc/win-pic.patch
	$(APPLY) $(SRC)/postproc/postproc-ranlib.patch
	$(MOVE)

.postproc: postproc
	$(REQUIRE_GPL)
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(EXTRA_CFLAGS)"  \
		--extra-ldflags="$(LDFLAGS)" $(POSTPROCCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
