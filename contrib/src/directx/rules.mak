# DirectX headers

DX_HEADERS_URL := $(CONTRIB_VIDEOLAN)/directx-oss.tar.bz2

ifdef HAVE_WIN32
PKGS += directx
endif

$(TARBALLS)/directx-oss.tar.bz2:
	$(DOWNLOAD) $(DX_HEADERS_URL)

.sum-directx: directx-oss.tar.bz2
	$(CHECK_SHA512)
	touch $@

.directx: directx-oss.tar.bz2 .sum-directx
	mkdir -p -- "$(PREFIX)/include"
	tar xvjf $< -C "$(PREFIX)/include"
	touch $@
