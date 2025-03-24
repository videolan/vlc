# generate Direct3D11 temporary include

D3D11_COMMIT_ID := f701c4c8cc9a881e660904f8c0047908a7b2ed04
D3D11_1_COMMIT_ID := aa6ab47929a9cac6897f38e630ce0bb88458e288
D3D11_4_COMMIT_ID := 6a1e782bb60bb1a93b5ab20fe895394d9c0904c2
DXGI12_COMMIT_ID := 3419b2d4b2b7e8b378696dc79546e3593f00ade6
DXGITYPE_COMMIT_ID := 3419b2d4b2b7e8b378696dc79546e3593f00ade6
DXGIDEBUG_COMMIT_ID := 22333acf22f89b9709c718467e04735157b5d27a
D3D11_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_COMMIT_ID)/tree/mingw-w64-headers/include/d3d11.h?format=raw
D3D11_1_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_1_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d11_1.h?format=raw
D3D11_2_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_1_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d11_2.h?format=raw
D3D11_3_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_1_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d11_3.h?format=raw
D3D11_4_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(D3D11_4_COMMIT_ID)/tree/mingw-w64-headers/direct-x/include/d3d11_4.h?format=raw
DXGI12_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi1_2.h?format=raw
DXGI13_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi1_3.h?format=raw
DXGI14_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi1_4.h?format=raw
DXGI15_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi1_5.h?format=raw
DXGI16_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGI12_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi1_6.h?format=raw
DXGITYPE_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGITYPE_COMMIT_ID)/tree/mingw-w64-headers/include/dxgitype.h?format=raw
DXGICOMMON_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGITYPE_COMMIT_ID)/tree/mingw-w64-headers/include/dxgicommon.h?format=raw
DXGIDEBUG_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGIDEBUG_COMMIT_ID)/tree/mingw-w64-headers/include/dxgidebug.h?format=raw
DXGIFORMAT_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGITYPE_COMMIT_ID)/tree/mingw-w64-headers/include/dxgiformat.h?format=raw
DXGI_H_URL := http://sourceforge.net/p/mingw-w64/mingw-w64/ci/$(DXGITYPE_COMMIT_ID)/tree/mingw-w64-headers/include/dxgi.h?format=raw


ifdef HAVE_WIN32
PKGS += d3d11
endif

$(TARBALLS)/d3d11.h:
	$(call download,$(D3D11_H_URL))

$(TARBALLS)/d3d11_1.h:
	$(call download,$(D3D11_1_H_URL))

$(TARBALLS)/d3d11_2.h:
	$(call download,$(D3D11_2_H_URL))

$(TARBALLS)/d3d11_3.h:
	$(call download,$(D3D11_3_H_URL))

$(TARBALLS)/d3d11_4.h:
	$(call download,$(D3D11_4_H_URL))

$(TARBALLS)/dxgi1_2.h:
	$(call download,$(DXGI12_H_URL))

$(TARBALLS)/dxgi1_3.h:
	$(call download,$(DXGI13_H_URL))

$(TARBALLS)/dxgi1_4.h:
	$(call download,$(DXGI14_H_URL))

$(TARBALLS)/dxgi1_5.h:
	$(call download,$(DXGI15_H_URL))

$(TARBALLS)/dxgi1_6.h:
	$(call download,$(DXGI16_H_URL))

$(TARBALLS)/dxgitype.h:
	$(call download,$(DXGITYPE_H_URL))

$(TARBALLS)/dxgicommon.h:
	$(call download,$(DXGICOMMON_H_URL))

$(TARBALLS)/dxgidebug.h:
	$(call download,$(DXGIDEBUG_H_URL))

$(TARBALLS)/dxgiformat.h:
	$(call download,$(DXGIFORMAT_H_URL))

$(TARBALLS)/dxgi.h:
	$(call download,$(DXGI_H_URL))

.sum-d3d11: $(TARBALLS)/d3d11.h $(TARBALLS)/d3d11_1.h $(TARBALLS)/d3d11_2.h $(TARBALLS)/d3d11_3.h $(TARBALLS)/d3d11_4.h $(TARBALLS)/dxgi1_2.h $(TARBALLS)/dxgi1_3.h $(TARBALLS)/dxgi1_4.h $(TARBALLS)/dxgi1_5.h $(TARBALLS)/dxgi1_6.h $(TARBALLS)/dxgitype.h $(TARBALLS)/dxgicommon.h $(TARBALLS)/dxgidebug.h $(TARBALLS)/dxgiformat.h $(TARBALLS)/dxgi.h

d3d11: .sum-d3d11
	mkdir -p $@
	cp $(TARBALLS)/d3d11.h $(TARBALLS)/d3d11_1.h $(TARBALLS)/d3d11_2.h $(TARBALLS)/d3d11_3.h $(TARBALLS)/d3d11_4.h $(TARBALLS)/dxgi1_2.h $(TARBALLS)/dxgi1_3.h $(TARBALLS)/dxgi1_4.h $(TARBALLS)/dxgi1_5.h $(TARBALLS)/dxgi1_6.h $(TARBALLS)/dxgitype.h $(TARBALLS)/dxgicommon.h $(TARBALLS)/dxgidebug.h $(TARBALLS)/dxgiformat.h $(TARBALLS)/dxgi.h $@
	touch $@

.d3d11: d3d11
	mkdir -p -- "$(PREFIX)/include/"
	cp $(TARBALLS)/d3d11.h $(TARBALLS)/d3d11_1.h $(TARBALLS)/d3d11_2.h $(TARBALLS)/d3d11_3.h $(TARBALLS)/d3d11_4.h $(TARBALLS)/dxgi1_2.h $(TARBALLS)/dxgi1_3.h $(TARBALLS)/dxgi1_4.h $(TARBALLS)/dxgi1_5.h $(TARBALLS)/dxgi1_6.h $(TARBALLS)/dxgitype.h $(TARBALLS)/dxgicommon.h $(TARBALLS)/dxgidebug.h $(TARBALLS)/dxgiformat.h $(TARBALLS)/dxgi.h "$(PREFIX)/include/"
	touch $@
