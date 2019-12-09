# lame

LAME_VERSION := 3.100
LAME_URL := $(SF)/lame/lame-$(LAME_VERSION).tar.gz
LAME_CFLAGS := $(CFLAGS)

$(TARBALLS)/lame-$(LAME_VERSION).tar.gz:
	$(call download_pkg,$(LAME_URL),lame)

.sum-lame: lame-$(LAME_VERSION).tar.gz

ifdef WITH_OPTIMIZATION
LAME_CFLAGS += -DNDEBUG
endif

lame: lame-$(LAME_VERSION).tar.gz .sum-lame
	$(UNPACK)
	$(APPLY) $(SRC)/lame/lame-forceinline.patch
	$(APPLY) $(SRC)/lame/sse.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/lame/struct-float-copy.patch
endif
	# Avoid relying on iconv.m4 from gettext, when reconfiguring.
	# This is only used by the frontend which we disable.
	cd $(UNPACK_DIR) && sed -i.orig 's/^AM_ICONV/#&/' configure.in
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.lame: lame
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(LAME_CFLAGS)" ./configure $(HOSTCONF) --disable-analyzer-hooks --disable-decoder --disable-gtktest --disable-frontend
	cd $< && $(MAKE) install
	touch $@
