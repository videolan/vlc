# generate Direct3D9 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INC_PATH = /usr/include/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INC_PATH = /`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

D3D9CAPS_COMMIT_ID := 477108e5706e73421634436c21cb76e1795b3609
DXVA2API_COMMIT_ID := 67bb96f54d720ca9e5aaa5da7d385348e0bfac31
D3D9CAPS_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9caps.h?format=raw
D3D9_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9.h?format=raw
DXVA2API_IDL_URL := https://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXVA2API_COMMIT_ID)/tree/mingw-w64-headers/include/dxva2api.idl?format=raw
DST_D3D9CAPS_H = $(PREFIX)/include/d3d9caps.h
DST_D3D9_H = $(PREFIX)/include/d3d9.h
DST_DXVA2API_H = $(PREFIX)/include/dxva2api.h


ifdef HAVE_WIN32
ifndef HAVE_VISUALSTUDIO
PKGS += d3d9
endif
endif

$(TARBALLS)/d3d9caps.h:
	$(call download_pkg,$(D3D9CAPS_H_URL),d3d9)

$(TARBALLS)/d3d9.h:
	$(call download_pkg,$(D3D9_H_URL),d3d9)

$(TARBALLS)/dxva2api.idl:
	$(call download_pkg,$(DXVA2API_IDL_URL),d3d9)

.sum-d3d9: $(TARBALLS)/d3d9caps.h $(TARBALLS)/d3d9.h $(TARBALLS)/dxva2api.idl

$(DST_D3D9CAPS_H): $(TARBALLS)/d3d9caps.h .sum-d3d9
	mkdir -p -- "$(PREFIX)/include/"
	cd $(TARBALLS) && patch -fp1 < $(SRC)/d3d9/d3d9caps.patch -o $@

$(DST_D3D9_H): $(TARBALLS)/d3d9.h .sum-d3d9
	mkdir -p -- "$(PREFIX)/include/"
	cd $(TARBALLS) && patch -fp1 < $(SRC)/d3d9/d3d9.patch -o $@

dxva2api/dxva2api.idl: .sum-d3d9
	mkdir -p dxva2api
	cp $(TARBALLS)/dxva2api.idl $@
	patch -fp1 < $(SRC)/d3d9/dxva2api.patch

$(DST_DXVA2API_H): dxva2api/dxva2api.idl
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -D_D3D9_H_ -D__C89_NAMELESS -I$(IDL_INC_PATH) -h -o $@ $<

.d3d9caps: $(DST_D3D9CAPS_H)
	touch $@

.d3d9: $(DST_D3D9_H) $(DST_D3D9CAPS_H) $(DST_DXVA2API_H)
	touch $@
