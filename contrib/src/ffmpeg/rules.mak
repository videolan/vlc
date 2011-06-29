# FFmpeg

FFMPEG_VERSION=0.4.8
FFMPEG_URL=$(SF)/ffmpeg/ffmpeg-$(FFMPEG_VERSION).tar.gz
FFMPEG_SVN=svn://svn.ffmpeg.org/ffmpeg/trunk
FFMPEG_SVN_REV=26400


FFMPEGCONF = --cc="$(CC)" \
	--disable-doc \
	--disable-decoder=libvpx \
	--disable-debug \
	--enable-gpl \
	--enable-postproc \
	--disable-ffprobe \
	--disable-ffserver \
	--disable-ffmpeg \
	--disable-ffplay \
	--disable-devices \
	--disable-protocols \
	--disable-avfilter \
	--disable-network

# Optional dependencies
ifdef BUILD_ENCODERS
# TODO:
#FFMPEGCONF+= --enable-libmp3lame
#.ffmpeg: .lame
#PKGS += lame
else
FFMPEGCONF += --disable-encoders --disable-muxers
# XXX: REVISIT --enable-small ?
endif

#FFMPEGCONF+= --enable-libgsm
#.ffmpeg: .gsm
#PKGS += gsm

#FFMPEGCONF += --enable-libvpx
#.ffmpeg: .vpx
#PKGS += vpx

# XXX: REVISIT
#ifndef HAVE_FPU
#FFMPEGCONF+= --disable-mpegaudio-hp
#endif

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --enable-cross-compile --cross-prefix=$(HOST)-
endif

# ARM stuff
ifeq ($(ARCH),arm)
FFMPEGCONF += --disable-runtime-cpudetect
endif

# Darwin
ifdef HAVE_DARWIN_OS
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin
FFMPEG_CFLAGS += -DHAVE_LRINTF
endif
ifdef HAVE_MACOSX
ifneq ($(findstring $(ARCH),i386 x86_64),)
FFMPEGCONF += --enable-memalign-hack
endif
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
.ffmpeg: .yasm
PKGS += yasm
endif

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic
endif

# Windows
ifdef HAVE_WIN32
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads \
	--disable-bzlib --disable-bsfs \
	--disable-decoder=dca --disable-encoder=vorbis

ifdef HAVE_WIN64
FFMPEGCONF += --disable-dxva2

FFMPEGCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
FFMPEGCONF += --enable-dxva2
.ffmpeg: .directx
PKGS += directx

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

ffmpeg-$(FFMPEG_VERSION).tar.gz:
	$(error FFmpeg snapshot is too old, VCS must be used!)
	$(call download,$(FFMPEG_URL))

$(TARBALLS)/ffmpeg-svn.tar.gz:
	$(SVN) export $(FFMPEG_SVN) ffmpeg-svn
	tar cvz ffmpeg-svn > $@

FFMPEG_VERSION := svn

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz .sum-ffmpeg
	$(UNPACK)
ifdef HAVE_WIN64
	(cd $@-$(FFMPEG_VERSION)/libswscale && patch -p0) < $(SRC)/ffmpeg/ffmpeg-win64.patch
endif
ifdef HAVE_WIN32
	sed -i "s/std=c99/std=gnu99/" $@-$(FFMPEG_VERSION)/configure
endif
	(cd $@-$(FFMPEG_VERSION) && patch -p1) < $(SRC)/ffmpeg/libavformat-ape.c.patch
	mv $@-$(FFMPEG_VERSION) $@
	touch $@

.ffmpeg: ffmpeg
	# TODO: .zlib
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(FFMPEG_CFLAGS) -DHAVE_STDINT_H"  \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
