# CrystalHD headers

CRYSTAL_HEADERS_URL := http://www.broadcom.com/docs/support/crystalhd/crystalhd_lgpl_includes_v1.zip

ifdef HAVE_WIN32
PKGS += crystalhd
endif

$(TARBALLS)/crystalhd_lgpl_includes_v1.zip:
	$(call download_pkg,$(CRYSTAL_HEADERS_URL),crystalhd)

CRYSTAL_SOURCES := crystalhd_lgpl_includes_v1.zip

.sum-crystalhd: $(CRYSTAL_SOURCES)

.crystalhd: $(CRYSTAL_SOURCES) .sum-crystalhd
	mkdir -p -- "$(PREFIX)/include/libcrystalhd"
	unzip -o $< -d "$(PREFIX)/include/libcrystalhd"
ifdef HAVE_WIN32 # we want dlopening on win32
	rm -rf $(PREFIX)/include/libcrystalhd/bc_drv_if.h
endif
	touch $@
