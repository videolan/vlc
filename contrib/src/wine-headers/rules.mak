# WINE
WINE_VERSION := 6.13
WINE_URL := https://dl.winehq.org/wine/source/6.x/wine-$(WINE_VERSION).tar.xz

ifdef HAVE_WIN32
PKGS += wine-headers
endif
ifeq ($(call mingw_at_least, 8), true)
PKGS_FOUND += wine-headers
endif

# Order is important since *_(n).idl will depends on *_(n-1).idl
WINE_IDL_HEADERS = \
	d3d11.idl \
	d3d11_1.idl d3d11_2.idl d3d11_3.idl d3d11_4.idl \
	dxgicommon.idl dxgitype.idl dxgiformat.idl \
	dxgidebug.idl \
	dxgi.idl dxgi1_2.idl dxgi1_3.idl dxgi1_4.idl dxgi1_5.idl dxgi1_6.idl \
	dxva2api.idl

$(TARBALLS)/wine-$(WINE_VERSION).tar.xz:
	$(call download_pkg,$(WINE_URL),wine)

.sum-wine-headers: wine-$(WINE_VERSION).tar.xz

wine-headers: wine-$(WINE_VERSION).tar.xz .sum-wine-headers
	$(UNPACK)
	$(MOVE)

wine_widl = echo "GEN $(1)" && \
	$(WIDL) -DBOOL=WINBOOL $(3) \
	-I$(PREFIX)/include -Iwine-headers/idl-include \
	-I`echo $(MSYSTEM) | tr A-Z a-z`/$(BUILD)/include -h \
	-o "$(PREFIX)/include/$(1)" "wine-headers/idl-include/$(2)"

.wine-headers: wine-headers
	@mkdir -p $(PREFIX)/include
	@mkdir -p wine-headers/idl-include
	@cp wine-headers/include/*.idl  wine-headers/idl-include # be sure to not use .h from the wine project
	@for header in $(WINE_IDL_HEADERS); do \
		$(call wine_widl,"`basename $$header idl`h",$$header,); \
	done
	touch $@
