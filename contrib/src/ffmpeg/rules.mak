# FFmpeg

#FFMPEG_SNAPURL := http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=HEAD;sf=tgz
FFMPEG_SNAPURL := http://git.libav.org/?p=libav.git;a=snapshot;h=HEAD;sf=tgz

FFMPEGCONF = \
	--cc="$(CC)" \
	--disable-doc \
	--disable-decoder=libvpx \
	--disable-decoder=bink \
	--enable-libgsm \
	--enable-libopenjpeg \
	--disable-debug \
	--disable-avdevice \
	--disable-devices \
	--disable-avfilter \
	--disable-filters

# Those tools are named differently in FFmpeg and Libav
#	--disable-ffserver \
#	--disable-ffplay \
#	--disable-ffprobe
DEPS_ffmpeg = zlib gsm openjpeg

# Optional dependencies
ifdef BUILD_ENCODERS
FFMPEGCONF += --enable-libmp3lame --enable-libvpx
DEPS_ffmpeg += lame $(DEPS_lame) vpx $(DEPS_vpx)
else
FFMPEGCONF += --disable-encoders --disable-muxers
endif

ifdef ENABLE_SMALL
FFMPEGCONF += --enable-small --optflags=-O2
ifeq ($(ARCH),arm)
ifdef HAVE_NEON
# XXX: assumes CPU >= cortex-a8, and thus thumb2 able
FFMPEGCONF += --enable-thumb
endif
endif
endif

# XXX: REVISIT
#ifndef HAVE_FPU
#FFMPEGCONF += --disable-mpegaudio-hp
#endif

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --enable-cross-compile
ifndef HAVE_IOS
FFMPEGCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
FFMPEGCONF += --disable-runtime-cpudetect --arch=arm
ifdef HAVE_NEON
FFMPEGCONF += --cpu=cortex-a8 --enable-neon
FFMPEG_CFLAGS +=-mfloat-abi=softfp -mfpu=neon
endif
endif

# x86 stuff
ifeq ($(ARCH),i386)
FFMPEGCONF += --arch=x86
endif

# Darwin
ifdef HAVE_DARWIN_OS
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin
FFMPEG_CFLAGS += -DHAVE_LRINTF
ifneq ($(findstring $(ARCH),i386 x86_64),)
FFMPEGCONF += --enable-memalign-hack
endif
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
endif
ifdef HAVE_IOS
FFMPEGCONF += --enable-pic --as="$(AS)" --disable-decoder=snow
ifeq ($(ARCH), arm)
FFMPEGCONF += --cpu=cortex-a8
endif
endif

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic
endif

# Windows
ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
DEPS_ffmpeg += directx
endif
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads \
	--disable-bzlib --disable-bsfs \
	--disable-decoder=dca --disable-encoder=vorbis \
	--enable-dxva2

ifdef HAVE_WIN64
FFMPEGCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
FFMPEGCONF+= --cpu=i686 --arch=x86
endif
else
FFMPEGCONF += --enable-pthreads
endif

ifdef HAVE_WINCE
FFMPEGCONF += --target-os=mingw32ce --arch=armv4l --cpu=armv4t \
	--disable-decoder=snow --disable-decoder=vc9 \
	--disable-decoder=wmv3 --disable-decoder=vorbis \
	--disable-decoder=dvdsub --disable-decoder=dvbsub
endif

FFMPEG_CFLAGS += --std=gnu99

# Build

PKGS += ffmpeg
ifeq ($(call need_pkg,"libavcodec >= 52.25.0 libavformat >= 52.30.0 libswscale"),)
PKGS_FOUND += ffmpeg
endif

$(TARBALLS)/ffmpeg-git.tar.gz:
	$(call download,$(FFMPEG_SNAPURL))

FFMPEG_VERSION := git

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz .sum-ffmpeg
	rm -Rf $@ $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
ifdef HAVE_WIN32
	sed -i "s/std=c99/std=gnu99/" $@-$(FFMPEG_VERSION)/configure
endif
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(FFMPEG_CFLAGS) -DHAVE_STDINT_H"  \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
