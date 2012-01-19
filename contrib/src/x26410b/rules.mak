# x264

X264_VERSION := 20050609
X264_URL := $(CONTRIB_VIDEOLAN)/x264-$(X264_VERSION).tar.gz
X264_GITURL := git://git.videolan.org/x264.git
X264_SNAPURL := http://git.videolan.org/?p=x264.git;a=snapshot;h=HEAD;sf=tgz

ifeq ($(call need_pkg,"x26410b"),)
PKGS_FOUND += x26410b
endif
DEPS_x264 =

X264CONF = --prefix="$(PREFIX)" --host="$(HOST)" \
	--enable-static \
	--bit-depth=10 \
	--disable-avs \
	--disable-lavf \
	--disable-cli \
	--disable-ffms
ifndef HAVE_WIN32
X264CONF += --enable-pic
else
X264CONF += --enable-win32thread
endif

$(TARBALLS)/x26410b-$(X264_VERSION).tar.gz:
	$(call download,$(X264_URL))

$(TARBALLS)/x26410b-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/x26410b-git.tar.gz:
	$(call download,$(X264_SNAPURL))

X264_VERSION := git

.sum-x26410b: x264-$(X264_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

x26410b: x26410b-$(X264_VERSION).tar.gz .sum-x26410b
	rm -Rf x26410b-git
	mkdir -p x26410b-git
	$(ZCAT) "$<" | (cd x26410b-git && tar xv --strip-components=1)
	$(MOVE)

.x26410b: x26410b
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && sed -i -e 's/libx264/libx26410b/g' Makefile config.mak
	cd $< && sed -i -e 's/x264/x26410b/g' x264.pc
	cd $< && mv x264.pc x26410b.pc
	cd $< && sed -i -e 's/x264.pc/x26410b.pc/g' Makefile
	cd $< && $(MAKE) install
	touch $@
