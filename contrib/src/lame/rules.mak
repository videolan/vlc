# lame

LAME_VERSION := 3.99
LAME_URL := $(SF)/lame/lame-$(LAME_VERSION).tar.gz

$(TARBALLS)/lame-$(LAME_VERSION).tar.gz:
	$(call download,$(LAME_URL))

.sum-lame: lame-$(LAME_VERSION).tar.gz

lame: lame-$(LAME_VERSION).tar.gz .sum-lame
	$(UNPACK)
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/lame/lame-win64.patch
endif
	$(MOVE)

.lame: lame
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-analyser-hooks --disable-decoder --disable-gtktest --disable-frontend
	cd $< && $(MAKE) install
	touch $@
