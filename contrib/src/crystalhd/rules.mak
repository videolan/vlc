# CrystalHD headers

CRYSTAL_HEADERS_URL := http://www.broadcom.com/docs/support/crystalhd/crystalhd_lgpl_includes_v1.zip

ifdef HAVE_WIN32
PKGS += crystalhd
endif

$(TARBALLS)/crystalhd_lgpl_includes_v1.zip:
	$(call download_pkg,$(CRYSTAL_HEADERS_URL),crystalhd)

CRYSTAL_SOURCES := crystalhd_lgpl_includes_v1.zip

.sum-crystalhd: $(CRYSTAL_SOURCES)

libcrystalhd: $(CRYSTAL_SOURCES) .sum-crystalhd
	$(RM) -R $(UNPACK_DIR) && unzip -o $< -d $(UNPACK_DIR)
	chmod -R u+w $(UNPACK_DIR)
	$(APPLY) $(SRC)/crystalhd/callback_proto.patch
ifdef HAVE_WIN32 # we want dlopening on win32
	rm -rf $(UNPACK_DIR)/bc_drv_if.h
endif
	$(MOVE)

.crystalhd: libcrystalhd
	rm -Rf "$(PREFIX)/include/libcrystalhd"
	cp -R $< "$(PREFIX)/include"
	touch $@
