# generate Direct3D9 temporary include

D3D9CAPS_COMMIT_ID := 477108e5706e73421634436c21cb76e1795b3609
DXVA2API_COMMIT_ID := 45def5d7a10885dfb87af3c7996f8de7197183b5
D3D9CAPS_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9caps.h?format=raw
D3D9_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9.h?format=raw
DXVA2API_H_URL := https://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXVA2API_COMMIT_ID)/tree/mingw-w64-headers/include/dxva2api.h?format=raw
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

$(TARBALLS)/dxva2api.h:
	$(call download_pkg,$(DXVA2API_H_URL),d3d9)

.sum-d3d9: $(TARBALLS)/d3d9caps.h $(TARBALLS)/d3d9.h $(TARBALLS)/dxva2api.h

$(DST_D3D9CAPS_H): $(TARBALLS)/d3d9caps.h .sum-d3d9
	mkdir -p -- "$(PREFIX)/include/"
	(cd $(TARBALLS) && patch -fp1 -o $@) < $(SRC)/d3d9/d3d9caps.patch

$(DST_D3D9_H): $(TARBALLS)/d3d9.h .sum-d3d9
	mkdir -p -- "$(PREFIX)/include/"
	(cd $(TARBALLS) && patch -fp1 -o $@) < $(SRC)/d3d9/d3d9.patch

$(DST_DXVA2API_H): .sum-d3d9
	mkdir -p -- "$(PREFIX)/include/"
	cp $(TARBALLS)/dxva2api.h $@

.d3d9caps: $(DST_D3D9CAPS_H)
	touch $@

.d3d9: $(DST_D3D9_H) $(DST_D3D9CAPS_H) $(DST_DXVA2API_H)
	touch $@
