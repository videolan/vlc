# POSTPROC

POSTPROC_SNAPURL := http://git.videolan.org/?p=libpostproc.git;a=snapshot;h=HEAD;sf=tgz

POSTPROCCONF = \
	--cc="$(CC)" \
	--disable-debug \
	--enable-gpl \
	--enable-postproc

DEPS_postproc = ffmpeg

ifdef ENABLE_SMALL
POSTPROCCONF += --enable-small --optflags=-O2
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
ifdef HAVE_NEON
POSTPROCCONF += --cpu=cortex-a8 --enable-neon
POSTPROC_CFLAGS +=-mfloat-abi=softfp -mfpu=neon
endif
endif

# x86 stuff
ifeq ($(ARCH),i386)
POSTPROCCONF += --arch=x86
endif

# Darwin
ifdef HAVE_DARWIN_OS
POSTPROCCONF += --arch=$(ARCH) --target-os=darwin
endif
ifdef HAVE_IOS
POSTPROCCONF += --as="$(AS)"
ifeq ($(ARCH),arm)
POSTPROCCONF += --cpu=cortex-a8
endif
endif
ifeq ($(ARCH),x86_64)
POSTPROCCONF += --cpu=core2
endif

# Linux
ifdef HAVE_LINUX
POSTPROCCONF += --target-os=linux
# --enable-pic
endif

# Windows
ifdef HAVE_WIN32
POSTPROCCONF += --target-os=mingw32
ifdef HAVE_WIN64
POSTPROCCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
POSTPROCCONF+= --cpu=i686 --arch=x86
endif
else
POSTPROCCONF += --enable-pthreads
endif

ifdef HAVE_WINCE
POSTPROCCONF += --target-os=mingw32ce --arch=armv4l --cpu=armv4t
endif

POSTPROC_CFLAGS += --std=gnu99

# Build

PKGS += postproc
ifeq ($(call need_pkg,"libpostproc"),)
PKGS_FOUND += postproc
endif

$(TARBALLS)/postproc-git.tar.gz:
	$(call download,$(POSTPROC_SNAPURL))

POSTPROC_VERSION := git

.sum-postproc: $(TARBALLS)/postproc-$(POSTPROC_VERSION).tar.gz
	$(warning Not implemented.)
	touch $@

postproc: postproc-$(POSTPROC_VERSION).tar.gz .sum-postproc
	rm -Rf $@ $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
ifdef HAVE_WIN32
	sed -i "s/std=c99/std=gnu99/" $@-$(POSTPROC_VERSION)/configure
endif
	$(MOVE)

.postproc: postproc
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(POSTPROC_CFLAGS) -DHAVE_STDINT_H"  \
		--extra-ldflags="$(LDFLAGS)" $(POSTPROCCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
