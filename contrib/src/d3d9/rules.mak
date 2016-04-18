# generate Direct3D9 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INC_PATH = /usr/include/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INC_PATH = /`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

D3D9CAPS_COMMIT_ID := 477108e5706e73421634436c21cb76e1795b3609
D3D9_H_ID := 477108e5706e73421634436c21cb76e1795b3609
D3D9CAPS_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9caps.h?format=raw
D3D9_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D9CAPS_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d9.h?format=raw
DST_D3D9CAPS_H = $(PREFIX)/include/d3d9caps.h
DST_D3D9_H = $(PREFIX)/include/d3d9.h


ifdef HAVE_WIN32
PKGS += d3d9
endif

$(TARBALLS)/d3d9caps.h:
	$(call download,$(D3D9CAPS_H_URL))

$(TARBALLS)/d3d9.h:
	$(call download,$(D3D9_H_URL))

.sum-d3d9: $(TARBALLS)/d3d9caps.h $(TARBALLS)/d3d9.h

$(DST_D3D9CAPS_H): $(TARBALLS)/d3d9caps.h
	mkdir -p -- "$(PREFIX)/include/"
	cp $(TARBALLS)/d3d9caps.h $@ && cd "$(PREFIX)/include/" && patch -fp1 < ../$(SRC)/d3d9/d3d9caps.patch

$(DST_D3D9_H): $(TARBALLS)/d3d9.h
	mkdir -p -- "$(PREFIX)/include/"
	cp $(TARBALLS)/d3d9.h $@ && cd "$(PREFIX)/include/" && patch -fp1 < ../$(SRC)/d3d9/d3d9.patch

.d3d9caps: $(DST_D3D9CAPS_H)
	touch $@
    
.d3d9: $(DST_D3D9_H) $(DST_D3D9CAPS_H)
	touch $@
