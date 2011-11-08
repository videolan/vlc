# x264

X264_VERSION := 20050609
X264_URL := $(CONTRIB_VIDEOLAN)/x264-$(X264_VERSION).tar.gz
X264_GITURL := git://git.videolan.org/x264.git
X264_SNAPURL := http://git.videolan.org/?p=x264.git;a=snapshot;h=HEAD;sf=tgz

ifdef BUILD_ENCODERS
PKGS += x264
endif
ifeq ($(call need_pkg,"x264 >= 0.86"),)
PKGS_FOUND += x264
endif
DEPS_x264 =

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

$(TARBALLS)/x264-$(X264_VERSION).tar.gz:
	$(call download,$(X264_URL))

$(TARBALLS)/x264-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/x264-git.tar.gz:
	$(call download,$(X264_SNAPURL))

X264_VERSION := git

.sum-x264: x264-$(X264_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

x264: x264-$(X264_VERSION).tar.gz .sum-x264
	rm -Rf x264-git
	mkdir -p x264-git
	zcat "$<" | (cd x264-git && tar xv --strip-components=1)
	$(MOVE)

.x264: x264
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && $(MAKE) install
	touch $@
