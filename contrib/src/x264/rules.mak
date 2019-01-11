# x264

X264_GITURL := git://git.videolan.org/x264.git
X264_SNAPURL := http://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-20180324-2245.tar.bz2
X262_GITURL := git://git.videolan.org/x262.git
X264_BASENAME := $(notdir $(X264_SNAPURL))

ifdef BUILD_ENCODERS
ifdef GPL
PKGS += x264
endif
endif

ifeq ($(call need_pkg,"x264 >= 0.148"),)
PKGS_FOUND += x264
endif

ifeq ($(call need_pkg,"x264 >= 0.153"),)
PKGS_FOUND += x26410b
endif

#ifeq ($(call need_pkg,"x262"),)
#PKGS_FOUND += x262
#endif

PKGS_ALL += x26410b

X264CONF = --prefix="$(PREFIX)" --host="$(HOST)" \
	--enable-static \
	--disable-avs \
	--disable-lavf \
	--disable-cli \
	--disable-ffms \
	--disable-opencl
ifndef HAVE_WIN32
X264CONF += --enable-pic
else
ifdef HAVE_WINSTORE
X264CONF += --enable-win32thread
else
X264CONF += --disable-win32thread
endif
ifeq ($(ARCH), arm)
X264_AS = AS="./tools/gas-preprocessor.pl -arch arm -as-type clang -force-thumb -- $(CC) -mimplicit-it=always"
endif
ifeq ($(ARCH),aarch64)
# Configure defaults to gas-preprocessor + armasm64 for this target,
# unless overridden.
X264_AS = AS="$(CC)"
endif
endif
ifdef HAVE_CROSS_COMPILE
X264CONF += --cross-prefix="$(HOST)-"
ifdef HAVE_ANDROID
# broken text relocations
ifeq ($(ANDROID_ABI), x86)
X264CONF += --disable-asm
endif
ifeq ($(ANDROID_ABI), x86_64)
X264CONF += --disable-asm
endif
endif
endif

$(TARBALLS)/x262-git.tar.xz:
	$(call download_git,$(X262_GITURL))

$(TARBALLS)/x262-git.tar.gz:
	$(call download,$(X262_SNAPURL))

$(TARBALLS)/x264-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/$(X264_BASENAME):
	$(call download,$(X264_SNAPURL))

.sum-x262: x262-git.tar.gz
	$(warning $@ not implemented)
	touch $@

.sum-x26410b: .sum-x264
	touch $@

.sum-x264: $(X264_BASENAME)

x264 x26410b: %: $(X264_BASENAME) .sum-%
	rm -Rf $*-git
	mkdir -p $*-git
	tar xvjfo "$<" --strip-components=1 -C $*-git
	$(UPDATE_AUTOCONFIG)
	mv $*-git $*

x262: x262-git.tar.gz .sum-x262
	rm -Rf $@-git
	mkdir -p $@-git
	tar xvzfo "$<" --strip-components=1 -C $@-git
	$(UPDATE_AUTOCONFIG)
	$(MOVE)


.x264: x264
	$(REQUIRE_GPL)
	cd $< && $(HOSTVARS) $(X264_AS) ./configure $(X264CONF)
	cd $< && $(MAKE) install
	touch $@

.x26410b: .x264
	touch $@

.x262: x262
	$(REQUIRE_GPL)
	cd $< && sed -i -e 's/x264/x262/g' configure
	cd $< && sed -i -e 's/x264_config/x262_config/g' *.h Makefile *.c
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && sed -i -e 's/x264.pc/x262.pc/g' Makefile
	cd $< && sed -i -e 's/x264.h/x262.h/g' Makefile
	cd $< && $(MAKE)
	cd $< && cp x264.h x262.h
	cd $< && $(MAKE) install
	touch $@
