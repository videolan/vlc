# DirectX headers, missing from mingw32

DSHOW_HEADERS_URL := $(CONTRIB_VIDEOLAN)/dshow-headers-oss.tar.bz2
D2D_HASH := b1affb70c021200b410eccd377ad5aeef2c5a85b
D2D_URL := http://github.com/2of1/d2d1headers/archive/master.zip
# FIXME: ^ D2D not working

ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
PKGS += dshow
endif
endif

$(TARBALLS)/dshow-headers-oss.tar.bz2:
	$(call download,$(DSHOW_HEADERS_URL))

$(TARBALLS)/d2d_headers.zip:
	$(call download,$(D2D_URL))

DSHOW_SOURCES := dshow-headers-oss.tar.bz2 d2d_headers.zip

.sum-dshow: $(DSHOW_SOURCES)

.dshow: $(DSHOW_SOURCES) .sum-dshow
	mkdir -p -- "$(PREFIX)/include"
	tar xjf $< -C "$(PREFIX)/include" \
		--wildcards --no-anchored '*.h' --strip-components=1
	unzip -jf $(TARBALLS)/d2d_headers.zip -d "$(PREFIX)/include"
	touch $@
