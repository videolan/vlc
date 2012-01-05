# libmpeg2

LIBMPEG2_VERSION = 0.5.1
LIBMPEG2_URL := http://libmpeg2.sourceforge.net/files/libmpeg2-$(LIBMPEG2_VERSION).tar.gz

PKGS += libmpeg2
ifeq ($(call need_pkg,"libmpeg2"),)
PKGS_FOUND += libmpeg2
endif

$(TARBALLS)/libmpeg2-$(LIBMPEG2_VERSION).tar.gz:
	$(call download,$(LIBMPEG2_URL))

.sum-libmpeg2: libmpeg2-$(LIBMPEG2_VERSION).tar.gz

libmpeg2: libmpeg2-$(LIBMPEG2_VERSION).tar.gz .sum-libmpeg2
	$(UNPACK)
	$(APPLY) $(SRC)/libmpeg2/libmpeg2-arm-pld.patch
	$(APPLY) $(SRC)/libmpeg2/libmpeg2-mc-neon.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub .auto
	$(MOVE)

.libmpeg2: libmpeg2
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --without-x --disable-sdl
	cd $</libmpeg2 && make && make install
	cd $</include && make && make install
	touch $@
