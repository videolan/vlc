# DirectX headers, missing from mingw32

DX_HEADERS_URL := $(CONTRIB_VIDEOLAN)/directx-oss.tar.bz2
DXVA2_URL := $(CONTRIB_VIDEOLAN)/dxva2api.h

ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
PKGS += directx
endif
endif

$(TARBALLS)/directx-oss.tar.bz2:
	$(call download,$(DX_HEADERS_URL))

$(TARBALLS)/dxva2api.h:
	$(call download,$(DXVA2_URL))

.sum-directx: directx-oss.tar.bz2 dxva2api.h

.directx: directx-oss.tar.bz2 dxva2api.h .sum-directx
	mkdir -p -- "$(PREFIX)/include"
	tar xvjf $< -C "$(PREFIX)/include"
	$(CC) -E -include dxva2api.h - < /dev/null > /dev/null 2>&1 || cp $(TARBALLS)/dxva2api.h "$(PREFIX)/include/"
	touch $@
