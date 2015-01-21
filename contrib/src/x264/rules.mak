# x264

X264_GITURL := git://git.videolan.org/x264.git
X264_SNAPURL := ftp://ftp.videolan.org/pub/videolan/x264/snapshots/last_stable_x264.tar.bz2
X262_GITURL := git://git.videolan.org/x262.git

ifdef BUILD_ENCODERS
ifdef GPL
PKGS += x264
endif
endif

ifeq ($(call need_pkg,"x264 >= 0.86"),)
PKGS_FOUND += x264
endif

ifeq ($(call need_pkg,"x26410b"),)
PKGS_FOUND += x26410b
endif

ifeq ($(call need_pkg,"x262"),)
PKGS_FOUND += x262
endif


X264CONF = --prefix="$(PREFIX)" --host="$(HOST)" \
	--enable-static \
	--disable-avs \
	--disable-lavf \
	--disable-cli \
	--disable-ffms
ifndef HAVE_WIN32
X264CONF += --enable-pic
else
X264CONF += --enable-win32thread
endif
ifdef HAVE_CROSS_COMPILE
X264CONF += --cross-prefix="$(HOST)-"
endif

$(TARBALLS)/x262-git.tar.xz:
	$(call download_git,$(X262_GITURL))

$(TARBALLS)/x262-git.tar.gz:
	$(call download,$(X262_SNAPURL))

$(TARBALLS)/x26410b-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/x26410b-git.tar.bz2:
	$(call download,$(X264_SNAPURL))

$(TARBALLS)/x264-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/x264-git.tar.bz2:
	$(call download,$(X264_SNAPURL))

.sum-x262: x262-git.tar.gz
	$(warning $@ not implemented)
	touch $@

.sum-x26410b: x26410b-git.tar.bz2
	$(warning $@ not implemented)
	touch $@

.sum-x264: x264-git.tar.bz2
	$(warning $@ not implemented)
	touch $@

x264: x264-git.tar.bz2 .sum-x264
	rm -Rf $@-git
	mkdir -p $@-git
	$(BZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

x26410b: x26410b-git.tar.bz2 .sum-x26410b
	rm -Rf $@-git
	mkdir -p $@-git
	$(BZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

x262: x262-git.tar.gz .sum-x26410b
	rm -Rf $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)


.x264: x264
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && $(MAKE) install
	touch $@

.x26410b: x26410b
	cd $< && $(HOSTVARS) ./configure $(X264CONF) --bit-depth=10
	cd $< && sed -i -e 's/libx264/libx26410b/g' Makefile config.mak
	cd $< && sed -i -e 's/x264/x26410b/g' x264.pc
	cd $< && mv x264.pc x26410b.pc
	cd $< && sed -i -e 's/x264.pc/x26410b.pc/g' Makefile
	cd $< && $(MAKE) install
	touch $@

.x262: x262
	cd $< && sed -i -e 's/x264/x262/g' configure
	cd $< && sed -i -e 's/x264_config/x262_config/g' *.h Makefile *.c
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && sed -i -e 's/x264.pc/x262.pc/g' Makefile
	cd $< && sed -i -e 's/x264.h/x262.h/g' Makefile
	cd $< && $(MAKE)
	cd $< && cp x264.h x262.h
	cd $< && $(MAKE) install
	touch $@
