# generate Direct3D11 temporary include

ifdef HAVE_CROSS_COMPILE
IDL_INCLUDES = -I/usr/include/wine/windows/ -I/usr/include/wine/wine/windows/
else
#ugly way to get the default location of standard idl files
IDL_INCLUDES = -I/`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include
endif

DST_DXVAHD_H = $(PREFIX)/include/dxvahd.h

ifdef HAVE_WIN32
PKGS += dxvahd
endif

.sum-dxvahd: $(TARBALLS)/dxvahd.idl

$(TARBALLS)/dxvahd.idl: $(SRC)/dxvahd/dxvahd.idl
	cp $< $@

$(DST_DXVAHD_H): $(TARBALLS)/dxvahd.idl .sum-dxvahd
	mkdir -p -- "$(PREFIX)/include/"
	$(WIDL) -DBOOL=WINBOOL -I$(PREFIX)/include $(IDL_INCLUDES) -h -o $@ $<

.dxvahd: $(DST_DXVAHD_H)
	touch $@
