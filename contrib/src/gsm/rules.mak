# GSM
GSM_MAJVERSION := 1.0
GSM_MINVERSION := 22
GSM_URL := http://www.quut.com/gsm/gsm-$(GSM_MAJVERSION).$(GSM_MINVERSION).tar.gz

$(TARBALLS)/gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz:
	$(call download_pkg,$(GSM_URL),gsm)

.sum-gsm: gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz

gsm: gsm-$(GSM_MAJVERSION)-pl$(GSM_MINVERSION).tar.gz .sum-gsm
	$(UNPACK)
	# allow overriding hardcoded compiler variables
	sed -i.orig 's,^CC	,#CC,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,^LD	,#LD,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,^AR	,#AR,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,^RANLIB	,#RANLIB,' "$(UNPACK_DIR)/Makefile"
	# allow overriding hardcoded install variables
	sed -i.orig 's,GSM_INSTALL_ROOT =,GSM_INSTALL_ROOT ?=,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,GSM_INSTALL_INC =,GSM_INSTALL_INC ?=,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,GSM_INSTALL_MAN =,GSM_INSTALL_MAN ?=,' "$(UNPACK_DIR)/Makefile"
	# use the default make rules (use CPPFLAGS)
	sed -i.orig 's,^.c.o:,#.c.o:,' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,^		$$(CC),#		$$(CC),' "$(UNPACK_DIR)/Makefile"
	sed -i.orig 's,^		@-mv,#		@-mv,' "$(UNPACK_DIR)/Makefile"
	$(MOVE)

GSM_ENV := GSM_INSTALL_ROOT="$(PREFIX)" \
           GSM_INSTALL_INC="$(PREFIX)/include/gsm" \
           GSM_INSTALL_MAN="$(PREFIX)/share/man/man3"

.gsm: gsm
	install -d "$(PREFIX)/lib" "$(PREFIX)/include/gsm" "$(PREFIX)/share/man/man3"
	$(HOSTVARS_PIC) $(GSM_ENV) $(MAKE) -C $< gsminstall
	touch $@
