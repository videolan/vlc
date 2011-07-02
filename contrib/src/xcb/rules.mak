# X protocol C language Bindings

XCB_VERSION := 1.7
XCB_URL := http://xcb.freedesktop.org/dist/libxcb-$(XCB_VERSION).tar.bz2

$(TARBALLS)/libxcb-$(XCB_VERSION).tar.bz2:
	$(call download,$(XCB_URL))

.sum-xcb: libxcb-$(XCB_VERSION).tar.bz2

libxcb: libxcb-$(XCB_VERSION).tar.bz2 .sum-xcb
	$(UNPACK)
	$(MOVE)

XCBCONF := \
	--enable-composite \
	--disable-damage \
	--disable-dpms \
	--disable-dri2 \
	--disable-glx \
	--enable-randr \
	--disable-render \
	--disable-resource \
	--disable-screensaver \
	--disable-shape \
	--enable-shm \
	--disable-sync \
	--disable-xevie \
	--disable-xfixes \
	--disable-xfree86-dri \
	--disable-xinerama \
	--disable-xinput \
	--disable-xprint \
	--disable-selinux \
	--disable-xtest \
	--enable-xv \
	--disable-xvmc \
	$(HOSTCONF)

.xcb: libxcb
	cd $< && $(HOSTVARS) ./configure $(XCBCONF)
	cd $< && $(MAKE) install
	touch $@
