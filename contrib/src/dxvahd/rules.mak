# generate Direct3D11 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INCLUDES = -I/usr/include/wine/windows/ -I/usr/include/wine/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INCLUDES = -I/`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

ifdef HAVE_WIN32
PKGS += dxvahd
endif
ifeq ($(HAVE_MINGW64_V8),true)
PKGS_FOUND += dxvahd
endif

.sum-dxvahd: $(TARBALLS)/dxvahd.idl

$(TARBALLS)/dxvahd.idl: $(SRC)/dxvahd/dxvahd.idl
	cp $< $@

dxvahd: $(TARBALLS)/dxvahd.idl .sum-dxvahd
	mkdir -p $@
	cp $(TARBALLS)/dxvahd.idl $@

.dxvahd: dxvahd
	cd $< && $(WIDL) -DBOOL=WINBOOL -I$(PREFIX)/include $(IDL_INCLUDES) -h -o dxvahd.h dxvahd.idl
	mkdir -p -- "$(PREFIX)/include/"
	cd $< && cp dxvahd.h "$(PREFIX)/include/"
	touch $@
