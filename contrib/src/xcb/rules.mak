# X protocol C language Bindings

XCB_VERSION := 1.14
XCB_URL := http://xorg.freedesktop.org/archive/individual/lib/libxcb-$(XCB_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += xcb
endif
endif

ifeq ($(call need_pkg,"xcb >= 1.8 xcb-shm xcb-composite xcb-xv >= 1.1.90.1"),)
# xcb-randr >= 1.3 is not that useful
PKGS_FOUND += xcb
endif

$(TARBALLS)/libxcb-$(XCB_VERSION).tar.gz:
	$(call download_pkg,$(XCB_URL),xcb)

.sum-xcb: libxcb-$(XCB_VERSION).tar.gz

libxcb: libxcb-$(XCB_VERSION).tar.gz .sum-xcb
	$(UNPACK)
	$(call pkg_static,"xcb.pc.in")
	$(MOVE)

XCBCONF := \
	--enable-composite \
	--disable-damage \
	--disable-dpms \
	--disable-dri2 \
	--disable-glx \
	--enable-randr \
	--enable-render \
	--disable-resource \
	--disable-screensaver \
	--enable-shape \
	--enable-shm \
	--disable-sync \
	--disable-xevie \
	--enable-xfixes \
	--disable-xfree86-dri \
	--disable-xinerama \
	--disable-xinput \
	--disable-xprint \
	--disable-selinux \
	--disable-xtest \
	--enable-xv \
	--disable-xvmc \
	--without-doxygen

DEPS_xcb = pthread-stubs xau $(DEPS_xau) xcb-proto $(DEPS_xcb-proto)

.xcb: libxcb
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(XCBCONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
