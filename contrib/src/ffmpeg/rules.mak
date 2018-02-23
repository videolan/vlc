# FFmpeg

#Uncomment the one you want
#USE_LIBAV ?= 1
#USE_FFMPEG ?= 1

ifndef USE_LIBAV
FFMPEG_HASH=eaff5fcb7cde8d1614755269773d471d3a3d1bfc
FFMPEG_SNAPURL := http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=$(FFMPEG_HASH);sf=tgz
FFMPEG_GITURL := http://git.videolan.org/git/ffmpeg.git
FFMPEG_LAVC_MIN := 57.37.100
USE_FFMPEG := 1
else
FFMPEG_HASH=e171022c24c42b1e88a51bb3b4c27f13c87c85cb
FFMPEG_SNAPURL := http://git.libav.org/?p=libav.git;a=snapshot;h=$(FFMPEG_HASH);sf=tgz
FFMPEG_GITURL := git://git.libav.org/libav.git
FFMPEG_LAVC_MIN := 57.16.0
endif

FFMPEG_BASENAME := $(subst .,_,$(subst \,_,$(subst /,_,$(FFMPEG_HASH))))

# bsf=vp9_superframe is needed to mux VP9 inside webm/mkv
FFMPEGCONF = \
	--cc="$(CC)" \
	--pkg-config="$(PKG_CONFIG)" \
	--disable-doc \
	--disable-encoder=vorbis \
	--disable-decoder=opus \
	--enable-libgsm \
	--disable-debug \
	--disable-avdevice \
	--disable-devices \
	--disable-avfilter \
	--disable-filters \
	--disable-protocol=concat \
	--disable-bsfs \
	--disable-bzlib \
	--disable-avresample \
	--enable-bsf=vp9_superframe

ifdef USE_FFMPEG
FFMPEGCONF += \
	--disable-swresample \
	--disable-iconv \
	--disable-avisynth \
	--disable-nvenc \
	--disable-linux-perf
ifdef HAVE_DARWIN_OS
FFMPEGCONF += \
	--disable-videotoolbox
endif
endif

DEPS_ffmpeg = zlib gsm

ifndef USE_LIBAV
FFMPEGCONF += \
	--enable-libopenjpeg
DEPS_ffmpeg += openjpeg
endif

# Optional dependencies
ifndef BUILD_NETWORK
FFMPEGCONF += --disable-network
endif
ifdef BUILD_ENCODERS
FFMPEGCONF += --enable-libmp3lame --enable-libvpx --disable-decoder=libvpx_vp8 --disable-decoder=libvpx_vp9
ifndef USE_FFMPEG
FFMPEGCONF += --disable-decoder=libvpx
endif
DEPS_ffmpeg += lame $(DEPS_lame) vpx $(DEPS_vpx)
else
FFMPEGCONF += --disable-encoders --disable-muxers
endif

# Small size
ifdef WITH_OPTIMIZATION
ifdef ENABLE_SMALL
FFMPEGCONF += --enable-small
endif
ifeq ($(ARCH),arm)
ifdef HAVE_ARMV7A
FFMPEGCONF += --enable-thumb
endif
endif
else
FFMPEGCONF += --optflags=-O0
endif

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --enable-cross-compile --disable-programs
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
FFMPEGCONF += --arch=arm
ifdef HAVE_NEON
FFMPEGCONF += --enable-neon
endif
ifdef HAVE_ARMV7A
FFMPEGCONF += --cpu=cortex-a8
endif
ifdef HAVE_ARMV6
FFMPEGCONF += --cpu=armv6 --disable-neon
endif
endif

# ARM64 stuff
ifeq ($(ARCH),aarch64)
FFMPEGCONF += --arch=aarch64
endif

# MIPS stuff
ifeq ($(ARCH),mipsel)
FFMPEGCONF += --arch=mips
endif
ifeq ($(ARCH),mips64el)
FFMPEGCONF += --arch=mips64
endif

# x86 stuff
ifeq ($(ARCH),i386)
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --arch=x86
endif
endif

# x86_64 stuff
ifeq ($(ARCH),x86_64)
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --arch=x86_64
endif
endif

# Darwin
ifdef HAVE_DARWIN_OS
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin
ifdef USE_FFMPEG
FFMPEGCONF += --disable-lzma
endif
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
ifdef HAVE_IOS
FFMPEGCONF += --enable-pic --extra-ldflags="$(EXTRA_CFLAGS)"
ifdef HAVE_NEON
FFMPEGCONF += --as="$(AS)"
endif
endif
endif

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic --extra-libs="-lm"

endif

ifdef HAVE_ANDROID
# broken text relocations
ifeq ($(ANDROID_ABI), x86)
FFMPEGCONF +=  --disable-mmx --disable-mmxext --disable-inline-asm
endif
ifeq ($(ANDROID_ABI), x86_64)
FFMPEGCONF +=  --disable-mmx --disable-mmxext --disable-inline-asm
endif
ifdef HAVE_NEON
ifeq ($(ANDROID_ABI), armeabi-v7a)
FFMPEGCONF += --as='gas-preprocessor.pl -as-type clang -arch arm $(CC)'
endif
endif
endif

# Windows
ifdef HAVE_WIN32
ifndef HAVE_VISUALSTUDIO
DEPS_ffmpeg += d3d11
ifndef HAVE_MINGW_W64
DEPS_ffmpeg += directx
endif
endif
FFMPEGCONF += --target-os=mingw32
FFMPEGCONF += --enable-w32threads
ifndef HAVE_WINSTORE
FFMPEGCONF += --enable-dxva2
else
FFMPEGCONF += --disable-dxva2
endif

ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=athlon64 --arch=x86_64
else
ifeq ($(ARCH),i386) # 32bits intel
FFMPEGCONF+= --cpu=i686 --arch=x86
else
ifdef HAVE_ARMV7A
FFMPEGCONF+= --arch=arm
endif
endif
endif

else # !Windows
FFMPEGCONF += --enable-pthreads
endif

# Solaris
ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
FFMPEGCONF += --target-os=sunos --enable-pic
endif

# Build
PKGS += ffmpeg
ifeq ($(call need_pkg,"libavcodec >= $(FFMPEG_LAVC_MIN) libavformat >= 53.21.0 libswscale"),)
PKGS_FOUND += ffmpeg
endif

FFMPEGCONF += --nm="$(NM)" --ar="$(AR)"

$(TARBALLS)/ffmpeg-$(FFMPEG_BASENAME).tar.xz:
	$(call download_git,$(FFMPEG_GITURL),,$(FFMPEG_HASH))

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(FFMPEG_BASENAME).tar.xz
	$(call check_githash,$(FFMPEG_HASH))
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_BASENAME).tar.xz .sum-ffmpeg
	rm -Rf $@ $@-$(FFMPEG_BASENAME)
	mkdir -p $@-$(FFMPEG_BASENAME)
	tar xvJf "$<" --strip-components=1 -C $@-$(FFMPEG_BASENAME)
ifdef USE_FFMPEG
	$(APPLY) $(SRC)/ffmpeg/armv7_fixup.patch
	$(APPLY) $(SRC)/ffmpeg/dxva_vc1_crash.patch
	$(APPLY) $(SRC)/ffmpeg/h264_early_SAR.patch
endif
ifdef USE_LIBAV
	$(APPLY) $(SRC)/ffmpeg/libav_gsm.patch
endif
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
