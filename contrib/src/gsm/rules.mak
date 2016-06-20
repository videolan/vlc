# GSM
GSM_VERSION := 1.0.13
GSM_URL := http://libgsm.sourcearchive.com/downloads/$(GSM_VERSION)/libgsm_$(GSM_VERSION).orig.tar.gz

$(TARBALLS)/libgsm_$(GSM_VERSION).tar.gz:
	$(call download_pkg,$(GSM_URL),gsm)

.sum-gsm: libgsm_$(GSM_VERSION).tar.gz

gsm: libgsm_$(GSM_VERSION).tar.gz .sum-gsm
	$(UNPACK)
	mv gsm-1.0-* libgsm_$(GSM_VERSION)
	$(APPLY) $(SRC)/gsm/gsm-cross.patch
	$(APPLY) $(SRC)/gsm/gsm-missing-include.patch
	sed -e 's/^CFLAGS.*=/CFLAGS+=/' -i.orig libgsm_$(GSM_VERSION)/Makefile
	$(MOVE)

.gsm: gsm
	cd $< && $(HOSTVARS_PIC) $(MAKE)
	mkdir -p "$(PREFIX)/include/gsm" "$(PREFIX)/lib"
	cp $</inc/gsm.h "$(PREFIX)/include/gsm/"
	cp $</lib/libgsm.a "$(PREFIX)/lib/"
	touch $@
