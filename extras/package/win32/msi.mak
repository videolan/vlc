if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/msi/config.wxi
endif

WIXPATH=`$(WIN32_PATH_CMD) -u 'C:\Program Files (x86)\Windows Installer XML v3.5\bin'`
HEAT=$(MSIDIR)/msi-heat.py
CANDLE=wine "$(WIXPATH)/candle.exe"
LIGHT=wine "$(WIXPATH)/light.exe"
VLCDIR=@PACKAGE_DIR@
MSIDIR=$(abs_srcdir)/extras/package/win32/msi
W_MSIDIR=`$(WIN32_PATH_CMD) -w '$(MSIDIR)'`
MSIBUILDDIR=$(abs_top_builddir)/extras/package/win32/msi
W_MSIBUILDDIR=`$(WIN32_PATH_CMD) -w '$(MSIBUILDDIR)'`
if HAVE_WIN64
MSIOUTFILE=vlc-$(VERSION)-win64.msi
else
MSIOUTFILE=vlc-$(VERSION)-win32.msi
endif

heat: package-win-strip
	$(HEAT) --dir $(VLCDIR)/plugins -cg CompPluginsGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(MSIBUILDDIR)/Plugins.fragment.wxs
	$(HEAT) --dir $(VLCDIR)/locale -cg CompLocaleGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(MSIBUILDDIR)/Locale.fragment.wxs
if BUILD_LUA
	$(HEAT) --dir $(VLCDIR)/lua -cg CompLuaGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(MSIBUILDDIR)/Lua.fragment.wxs
endif
if BUILD_SKINS
	$(HEAT) --dir $(VLCDIR)/skins -cg CompSkinsGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(MSIBUILDDIR)/Skins.fragment.wxs
endif

candle: heat
	$(am__cd) $(MSIBUILDDIR) && $(CANDLE) -arch $(WINDOWS_ARCH) -ext WiXUtilExtension $(W_MSIDIR)/product.wxs $(W_MSIDIR)/axvlc.wxs $(W_MSIDIR)/extensions.wxs $(W_MSIBUILDDIR)/*.fragment.wxs

$(MSIOUTFILE): candle
	$(AM_V_GEN)cd vlc-@VERSION@ && $(LIGHT) -sval -spdb -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -b $(W_MSIDIR) $(W_MSIBUILDDIR)/product.wixobj $(W_MSIBUILDDIR)/axvlc.wixobj $(W_MSIBUILDDIR)/extensions.wixobj $(W_MSIBUILDDIR)/*.fragment.wixobj -o ../$@
	chmod 644 $@

package-msi: $(MSIOUTFILE)

cleanmsi:
	-rm -f $(MSIBUILDDIR)/*.wixobj
	-rm -f $(MSIBUILDDIR)/*.wixpdb
	-rm -f $(MSIBUILDDIR)/*.fragment.wxs

distcleanmsi: cleanmsi
	-rm -f $(MSIOUTFILE)

.PHONY: heat candle cleanmsi distcleanmsi package-msi
