# FFmpeg

FFMPEG_HASH=d4b7b3c03ee2baf0166ce49dff17ec9beff684db
FFMPEG_BRANCH=release/4.4
FFMPEG_GITURL := http://git.videolan.org/git/ffmpeg.git
FFMPEG_LAVC_MIN := 57.37.100

FFMPEG_BASENAME := $(subst .,_,$(subst \,_,$(subst /,_,$(FFMPEG_HASH))))

# bsf=vp9_superframe is needed to mux VP9 inside webm/mkv
FFMPEGCONF = \
	--cc="$(CC)" \
	--pkg-config="$(PKG_CONFIG)" \
	--disable-doc \
	--disable-encoder=vorbis \
	--disable-decoder=opus \
	--enable-libgsm \
	--enable-libopenjpeg \
	--disable-debug \
	--disable-avdevice \
	--disable-devices \
	--disable-avfilter \
	--disable-filters \
	--disable-protocol=concat \
	--disable-bsfs \
	--disable-bzlib \
	--disable-libvpx \
	--disable-avresample \
	--enable-bsf=vp9_superframe \
	--disable-swresample \
	--disable-iconv \
	--disable-avisynth \
	--disable-nvenc \
	--disable-linux-perf
ifdef HAVE_DARWIN_OS
FFMPEGCONF += \
	--disable-securetransport
endif

DEPS_ffmpeg = zlib $(DEPS_zlib) gsm $(DEPS_gsm) openjpeg $(DEPS_openjpeg)

# Optional dependencies
ifndef BUILD_NETWORK
FFMPEGCONF += --disable-network
endif
ifdef BUILD_ENCODERS
FFMPEGCONF += --enable-libmp3lame
DEPS_ffmpeg += lame $(DEPS_lame)
else
FFMPEGCONF += --disable-encoders --disable-muxers
endif

# Postproc
MAYBE_POSTPROC =
ifdef GPL
FFMPEGCONF += --enable-gpl --enable-postproc
MAYBE_POSTPROC = libpostproc
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
FFMPEGCONF += --optflags=-Og
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

# RISC-V stuff
ifneq ($(findstring $(ARCH),riscv32 riscv64),)
FFMPEGCONF += --arch=riscv
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
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin --extra-cflags="$(CFLAGS)"
FFMPEGCONF += --disable-lzma
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
ifdef HAVE_IOS
FFMPEGCONF += --enable-pic --extra-ldflags="$(EXTRA_CFLAGS) -isysroot $(IOS_SDK)"
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
endif

# Windows
ifdef HAVE_WIN32
ifndef HAVE_VISUALSTUDIO
DEPS_ffmpeg += wine-headers
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

ifdef HAVE_EMSCRIPTEN
FFMPEGCONF+= --arch=wasm32 --target-os=emscripten
endif

# Build
PKGS += ffmpeg
ifeq ($(call need_pkg,"libavcodec >= $(FFMPEG_LAVC_MIN) libavformat >= 53.21.0 libswscale $(MAYBE_POSTPROC)"),)
PKGS_FOUND += ffmpeg
endif

FFMPEGCONF += --nm="$(NM)" --ar="$(AR)" --ranlib="$(RANLIB)"

$(TARBALLS)/ffmpeg-$(FFMPEG_BASENAME).tar.xz:
	$(call download_git,$(FFMPEG_GITURL),$(FFMPEG_BRANCH),$(FFMPEG_HASH))

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(FFMPEG_BASENAME).tar.xz
	$(call check_githash,$(FFMPEG_HASH))
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_BASENAME).tar.xz .sum-ffmpeg
	$(UNPACK)
	$(APPLY) $(SRC)/ffmpeg/armv7_fixup.patch
	$(APPLY) $(SRC)/ffmpeg/dxva_vc1_crash.patch
	$(APPLY) $(SRC)/ffmpeg/h264_early_SAR.patch
	$(APPLY) $(SRC)/ffmpeg/0001-avcodec-dxva2_hevc-add-support-for-parsing-HEVC-Rang.patch
	$(APPLY) $(SRC)/ffmpeg/0002-avcodec-hevcdec-allow-HEVC-444-8-10-12-bits-decoding.patch
	$(APPLY) $(SRC)/ffmpeg/0003-avcodec-hevcdec-allow-HEVC-422-10-12-bits-decoding-w.patch
	$(APPLY) $(SRC)/ffmpeg/0001-avcodec-mpeg12dec-don-t-call-hw-end_frame-when-start.patch
	$(APPLY) $(SRC)/ffmpeg/0002-avcodec-mpeg12dec-don-t-end-a-slice-without-first_sl.patch
	$(APPLY) $(SRC)/ffmpeg/0001-fix-mf_utils-compilation-with-mingw64.patch
	$(APPLY) $(SRC)/ffmpeg/0001-avcodec-vp9-Do-not-destroy-uninitialized-mutexes-con.patch
	$(APPLY) $(SRC)/ffmpeg/0001-ffmpeg-add-target_os-support-for-emscripten.patch
	$(APPLY) $(SRC)/ffmpeg/0001-dxva2_hevc-don-t-use-frames-as-reference-if-they-are.patch
	$(APPLY) $(SRC)/ffmpeg/0001-lavc-aarch64-fix-relocation-out-of-range-error.patch
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	$(MAKE) -C $< install-libs install-headers
	touch $@
