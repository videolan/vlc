# X protocol C language Bindings

XCB_VERSION := 1.12
XCB_URL := http://xcb.freedesktop.org/dist/libxcb-$(XCB_VERSION).tar.bz2

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += xcb
endif
endif

ifeq ($(call need_pkg,"xcb >= 1.6 xcb-shm xcb-composite xcb-xv >= 1.1.90.1"),)
# xcb-randr >= 1.3 is not that useful
PKGS_FOUND += xcb
endif

$(TARBALLS)/libxcb-$(XCB_VERSION).tar.bz2:
	$(call download,$(XCB_URL))

.sum-xcb: libxcb-$(XCB_VERSION).tar.bz2

libxcb: libxcb-$(XCB_VERSION).tar.bz2 .sum-xcb
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
	--without-doxygen \
	$(HOSTCONF)

DEPS_xcb = xau $(DEPS_xau) xcb-proto $(DEPS_xcb-proto)

.xcb: libxcb
	cd $< && $(HOSTVARS) ./configure $(XCBCONF)
	cd $< && $(MAKE) install
	touch $@
