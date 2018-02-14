# generate Direct3D11 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INC_PATH = /usr/include/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INC_PATH = /`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

D3D11_COMMIT_ID := a0cd5afeb60be3be0860e9a203314c10485bb9b8
DXGI12_COMMIT_ID := 790a6544347b53c314b9c6f1ea757a2d5504c67e
DXGITYPE_COMMIT_ID := f4aba520d014ecfe3563e33860de001caf2804e2
D3D11_IDL_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d11.idl?format=raw
DXGI12_IDL_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/dxgi1_2.idl?format=raw
DXGITYPE_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGITYPE_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/dxgitype.h?format=raw
DST_D3D11_H = $(PREFIX)/include/d3d11.h
DST_DXGIDEBUG_H = $(PREFIX)/include/dxgidebug.h
DST_DXGITYPE_H = $(PREFIX)/include/dxgitype.h
DST_DXGI12_H = $(PREFIX)/include/dxgi1_2.h
DST_DXGI13_H = $(PREFIX)/include/dxgi1_3.h
DST_DXGI14_H = $(PREFIX)/include/dxgi1_4.h
DST_DXGI15_H = $(PREFIX)/include/dxgi1_5.h
DST_DXGI16_H = $(PREFIX)/include/dxgi1_6.h


ifdef HAVE_WIN32
PKGS += d3d11
endif

$(TARBALLS)/d3d11.idl:
	$(call download_pkg,$(D3D11_IDL_URL),d3d11)

$(TARBALLS)/dxgidebug.idl:
	(cd $(TARBALLS) && patch -fp1) < $(SRC)/d3d11/dxgidebug.patch

$(TARBALLS)/dxgi1_2.idl:
	$(call download_pkg,$(DXGI12_IDL_URL),d3d11)

$(TARBALLS)/dxgitype.h:
	$(call download_pkg,$(DXGITYPE_H_URL),d3d11)

.sum-d3d11: $(TARBALLS)/d3d11.idl $(TARBALLS)/dxgidebug.idl $(TARBALLS)/dxgi1_2.idl $(TARBALLS)/dxgitype.h

d3d11: .sum-d3d11
	mkdir -p $@
	cp $(TARBALLS)/d3d11.idl $@ && cd $@ && patch -fp1 < ../$(SRC)/d3d11/processor_format.patch

dxgi12: .sum-d3d11
	mkdir -p $@
	cp $(TARBALLS)/dxgi1_2.idl $@ && cd $@ && patch -fp1 < ../$(SRC)/d3d11/dxgi12.patch

$(DST_D3D11_H): d3d11
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -I$(IDL_INC_PATH) -h -o $@ $</d3d11.idl

$(DST_DXGIDEBUG_H): $(TARBALLS)/dxgidebug.idl
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -I$(IDL_INC_PATH) -h -o $@ $<

$(DST_DXGITYPE_H): $(TARBALLS)/dxgitype.h
	mkdir -p -- "$(PREFIX)/include/"
	cp $(TARBALLS)/dxgitype.h $@

$(DST_DXGI12_H): dxgi12
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -I$(IDL_INC_PATH) -h -o $@ $</dxgi1_2.idl

$(DST_DXGI13_H): $(SRC)/d3d11/dxgi1_3.idl $(DST_DXGI12_H)
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -Idxgi12 -I$(IDL_INC_PATH) -h -o $@ $<

$(DST_DXGI14_H): $(SRC)/d3d11/dxgi1_4.idl $(DST_DXGI13_H)
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -Idxgi12 -I$(IDL_INC_PATH) -h -o $@ $<

$(DST_DXGI15_H): $(SRC)/d3d11/dxgi1_5.idl $(DST_DXGI14_H)
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -Idxgi12 -I$(IDL_INC_PATH) -h -o $@ $<

$(DST_DXGI16_H): $(SRC)/d3d11/dxgi1_6.idl $(DST_DXGI15_H)
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -Idxgi12 -I$(IDL_INC_PATH) -h -o $@ $<

.dxgitype: $(DST_DXGITYPE_H)
	touch $@

.dxgi12: .dxgitype $(DST_DXGI12_H)
	touch $@

.dxgi13: .dxgi12 $(DST_DXGI13_H)
	touch $@

.dxgi14: .dxgi13 $(DST_DXGI14_H)
	touch $@

.dxgi15: .dxgi14 $(DST_DXGI15_H)
	touch $@

.dxgi16: .dxgi15 $(DST_DXGI16_H)
	touch $@

.d3d11: $(DST_D3D11_H) $(DST_DXGIDEBUG_H) .dxgi16
	touch $@
