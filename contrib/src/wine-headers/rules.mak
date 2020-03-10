# WINE
WINE_VERSION := 5.3
WINE_URL := https://dl.winehq.org/wine/source/5.x/wine-$(WINE_VERSION).tar.xz

ifdef HAVE_WIN32
PKGS += wine-headers
endif

# Order is important since *_(n).idl will depends on *_(n-1).idl
WINE_EXTRA_HEADERS = dxva.h
WINE_IDL_HEADERS = \
	d3d11.idl \
	d3d11_1.idl d3d11_2.idl d3d11_3.idl \
	dxgicommon.idl dxgitype.idl dxgiformat.idl \
	dxgidebug.idl \
	dxgi.idl dxgi1_2.idl dxgi1_3.idl dxgi1_4.idl dxgi1_5.idl dxgi1_6.idl \
	dxva2api.idl

ifndef HAVE_VISUALSTUDIO
WINE_EXTRA_HEADERS += d3d9caps.h d3d9.h
endif

$(TARBALLS)/wine-$(WINE_VERSION).tar.xz:
	$(call download_pkg,$(WINE_URL),wine)

.sum-wine-headers: wine-$(WINE_VERSION).tar.xz

wine-headers: wine-$(WINE_VERSION).tar.xz .sum-wine-headers
	$(UNPACK)
	$(APPLY) $(SRC)/wine-headers/0001-include-add-HEVC-and-VP8-VP9-support-to-dxva.h.patch
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
	@for header in $(WINE_EXTRA_HEADERS); do \
		echo "CP  $$header"; \
		cp "wine-headers/include/$$header" "$(PREFIX)/include"; \
	done
	touch $@
