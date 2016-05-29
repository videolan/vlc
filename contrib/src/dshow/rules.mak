# DirectX headers, missing from mingw32

DSHOW_HEADERS_URL := $(CONTRIB_VIDEOLAN)/dshow-headers-oss.tar.bz2

ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
PKGS += dshow
endif
endif

$(TARBALLS)/dshow-headers-oss.tar.bz2:
	$(call download,$(DSHOW_HEADERS_URL))

DSHOW_SOURCES := dshow-headers-oss.tar.bz2

.sum-dshow: $(DSHOW_SOURCES)

.dshow: $(DSHOW_SOURCES) .sum-dshow
	mkdir -p -- "$(PREFIX)/include"
	tar xjf $< -C "$(PREFIX)/include" \
		--wildcards --no-anchored '*.h' --strip-components=1
	touch $@
