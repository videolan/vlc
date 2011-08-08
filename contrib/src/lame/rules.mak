# lame

LAME_VERSION := 3.98.4
LAME_URL := $(SF)/lame/lame-$(LAME_VERSION).tar.gz

ifdef BUILD_ENCODERS
PKGS += lame
endif

$(TARBALLS)/lame-$(LAME_VERSION).tar.gz:
	$(call download,$(LAME_URL))

.sum-lame: lame-$(LAME_VERSION).tar.gz

lame: lame-$(LAME_VERSION).tar.gz .sum-lame
	$(UNPACK)
	$(MOVE)

.lame: lame
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-analyser-hooks --disable-decoder --disable-gtktest --disable-frontend
	cd $< && $(MAKE) install
	touch $@
