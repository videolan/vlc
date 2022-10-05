# GSM
GSM_MAJVERSION := 1.0
GSM_MINVERSION := 13
GSM_URL := http://www.quut.com/gsm/gsm-$(GSM_MAJVERSION).$(GSM_MINVERSION).tar.gz

$(TARBALLS)/gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz:
	$(call download_pkg,$(GSM_URL),gsm)

.sum-gsm: gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz

gsm: gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz .sum-gsm
	$(UNPACK)
	$(APPLY) $(SRC)/gsm/gsm-cross.patch
	$(APPLY) $(SRC)/gsm/gsm-missing-include.patch
	sed -e 's/^CFLAGS.*=/CFLAGS+=/' -i.orig $(UNPACK_DIR)/Makefile
	$(MOVE)

.gsm: gsm
	$(HOSTVARS_PIC) $(MAKE) -C $<
	mkdir -p "$(PREFIX)/include/gsm" "$(PREFIX)/lib"
	cp $</inc/gsm.h "$(PREFIX)/include/gsm/"
	cp $</lib/libgsm.a "$(PREFIX)/lib/"
	touch $@
