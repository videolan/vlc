# DirectX headers

DSHOW_HEADERS_URL := $(CONTRIB_VIDEOLAN)/dshow-headers-oss.tar.bz2
DXVA2_URL := $(CONTRIB_VIDEOLAN)/dxva2api.h
D2D_HASH := b1affb70c021200b410eccd377ad5aeef2c5a85b
D2D_URL := http://nodeload.github.com/2of1/d2d1headers/tarball/master
# FIXME: ^ D2D not working

ifdef HAVE_WIN32
PKGS += dshow
endif

$(TARBALLS)/dshow-headers-oss.tar.bz2:
	$(call download,$(DSHOW_HEADERS_URL))

$(TARBALLS)/dxva2api.h:
	$(call download,$(DXVA2_URL))

$(TARBALLS)/d2d_headers.tar.gz:
	$(call download,$(D2D_URL) -O $@)

DSHOW_SOURCES := dshow-headers-oss.tar.bz2 dxva2api.h d2d_headers.tar.gz

.sum-dshow: $(DSHOW_SOURCES)

.dshow: $(DSHOW_SOURCES) .sum-dshow
	mkdir -p -- "$(PREFIX)/include"
	tar xjf $< -C "$(PREFIX)/include" \
		--wildcards --no-anchored '*.h' --strip-components=1
	tar xzf $(TARBALLS)/d2d_headers.tar.gz -C "$(PREFIX)/include" \
		 --wildcards --no-anchored '*.h' --strip-components=1
	cp $(TARBALLS)/dxva2api.h "$(PREFIX)/include"
	touch $@
